//This manages all of the outputting from the Boltzmann code

#include "output.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "momentRoutines.h"
#include "input.h"
#include <mpi.h>
#include "species.h"

/*********************
MPI STUFF
 *********************/
static int rank;
static int numNodes;

//File streams for output
static FILE **fidRho; 
static FILE **fidKinTemp;
static FILE **fidPressure;
static FILE **fidMarginal; 
static FILE **fidSlice;
static FILE **fidBulkV;
static FILE **fidEntropy;
static FILE **fidBGK;

//Parameters from main code
static int N;
static double L_v;
static int nX;
static int nX_node;
static double *x, *dx;

//flags for output, default set to output them
static int densFlag = 1;
static int velFlag = 1;
static int tempFlag = 1;
static int presFlag = 1;
static int marginalFlag = 1;
static int sliceFlag = 1;
static int entFlag = 1;
static int BGKFlag = 1;

static species *mixture;
static int Ns;

//Takes the Linf norm of the difference with maxwellian
double MaxConverge(int N, double *v, double rho, double* u, double T, double *f) {
  double max = 0, test = 0;
  int i,j,k;;
  for(i=0;i<N;i++)
    for(j=0;j<N;j++)
      for(k=0;k<N;k++) {
	test = fabs(f[k + N*(j + N*i)] - (rho/pow(2*M_PI*T,1.5))*exp(-( (v[i] - u[0])*(v[i] - u[0]) + (v[j] - u[1])*(v[j] - u[1]) + (v[k] - u[2])*(v[k] - u[2]))/(2*T)));
	if(test > max)
	  max = test;
      }
  return max;
}

//initializer for homogeneous case
void initialize_output_hom(int nodes, double Lv, int restart, char *inputFile, char *outputOptions, species *mix, int num_species) {

  int i;

  N = nodes;
  L_v = Lv;
  mixture = mix;
  Ns = num_species;

  char line[80] = {"dummy"};
  char path[100] = {"./input/"};

  strcat(path,outputOptions);

  //Load flag data from file
  FILE *outParams = fopen(path,"r");

  /*Read file*/
  while (strcmp(line,"Stop") != 0) {
    read_line(outParams,line);

    if(strcmp(line,"density") == 0) 
      densFlag = read_int(outParams);

    if(strcmp(line,"velocity") == 0)
      velFlag = read_int(outParams);

    if(strcmp(line,"temperature") == 0)
      tempFlag = read_int(outParams);

    if(strcmp(line,"pressure") == 0)
      presFlag = read_int(outParams);

    if(strcmp(line,"marginal") == 0)
      marginalFlag = read_int(outParams);

    if(strcmp(line,"slice") == 0)
      sliceFlag = read_int(outParams);

    if(strcmp(line,"entropy") == 0)
      entFlag = read_int(outParams);
  }

  fclose(outParams);

  printf("Opening streams\n");

  fidRho = malloc(Ns*sizeof(FILE *));
  fidKinTemp = malloc(Ns*sizeof(FILE *));
  fidPressure = malloc(Ns*sizeof(FILE *));
  fidBulkV = malloc(Ns*sizeof(FILE *));
  fidSlice = malloc(Ns*sizeof(FILE *));
  fidEntropy = malloc(Ns*sizeof(FILE *));
  fidBGK = malloc(Ns*sizeof(FILE *));
  
  //set up dynamically generated filenames + initialize file handles
  char buffer_rho [100];
  char buffer_kintemp [100];
  char buffer_pres [100];
  char buffer_bulkV [100];
  char buffer_slice[100];
  char buffer_entropy[100];
  char buffer_BGK[100];

  for(i=0;i<Ns;i++) {
      
    sprintf(buffer_rho,"Data/rho_%s_%s_%s.plt",inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_kintemp,"Data/kintemp_%s_%s_%s.plt",inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_pres,"Data/pres_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_bulkV,"Data/bulkV_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_slice,"Data/slice_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_entropy,"Data/entropy_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
    sprintf(buffer_BGK,"Data/BGK_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
    

    if(restart) {
      if(densFlag)
	fidRho[i] = fopen(buffer_rho, "a");

      if(tempFlag)
	fidKinTemp[i] = fopen(buffer_kintemp, "a");

      if(presFlag)
	fidPressure[i] = fopen(buffer_pres, "a");

      if(velFlag)
	fidBulkV[i] = fopen(buffer_bulkV, "a");


      if(sliceFlag)
	fidSlice[i] = fopen(buffer_slice, "a");

      if(entFlag)
	fidEntropy[i] = fopen(buffer_entropy,"a");
    
      if(BGKFlag) {
	fidBGK[i] = fopen(buffer_BGK,"a");
      }
      
    }
    else {
      if(densFlag)
	fidRho[i] = fopen(buffer_rho, "w");

      if(tempFlag)
	fidKinTemp[i] = fopen(buffer_kintemp, "w");

      if(presFlag)
	fidPressure[i] = fopen(buffer_pres, "w");

      if(velFlag)
	fidBulkV[i] = fopen(buffer_bulkV, "w");

      if(sliceFlag)
	fidSlice[i] = fopen(buffer_slice, "w");

      if(entFlag)
	fidEntropy[i] = fopen(buffer_entropy, "w");

      if(BGKFlag) {
	fidBGK[i] = fopen(buffer_BGK,"w");
      }
    }
  }
}

//This version is for homogeneous case
void write_streams(double **f, double time, double *v) {
  int i,l;
  double density, kinTemp, bulkV[3], half, three, energy[2];

  for(i=0;i<Ns;i++) {
    density = getDensity(f[i],0);
    if(isnan( density)) {
      printf("nan detected\n");
      exit(0);
    }
    getBulkVelocity(f[i], bulkV, density, 0);
    kinTemp = getTemperature(f[i], bulkV, density, 0);
    getEnergy(f[i],energy);
  
    if(densFlag) 
      fprintf(fidRho[i], "%le %le\n", time, density);
    if(velFlag)
      fprintf(fidBulkV[i], "%le %le\n", time, bulkV[0]);
    if(tempFlag)
      fprintf(fidKinTemp[i], "%le %le\n", time, kinTemp);
    if(presFlag)
      fprintf(fidPressure[i], "%le %le\n", time, getPressure(density, kinTemp));
    if(sliceFlag) {
      fprintf(fidSlice[i], "ZONE I=%d T=\"%g\"\n", N, time);
      for(l=0;l<N;l++) {
	fprintf(fidSlice[i], "%le %le\n", v[l], f[i][N/2 + N*(N/2 + N*l)]);
      }
      fprintf(fidSlice[i],"\n");
    }
    if(entFlag) {
      fprintf(fidEntropy[i], "%le %le\n", time, energy[1]/energy[0]);
    }
    /*    if(BGKFlag) {
      fprintf(fidBGK[i],"%g %g\n",time,);
      }*/

    if(densFlag) 
      fflush(fidRho[i]);
    if(velFlag)
      fflush(fidBulkV[i]);
    if(tempFlag)
      fflush(fidKinTemp[i]);
    if(presFlag)
      fflush(fidPressure[i]);
    if(entFlag)
      fflush(fidEntropy[i]);
    if(sliceFlag)
      fflush(fidSlice[i]);
  }
}
  
void initialize_output_inhom(int nodes, double Lv, int numX, int numX_node, double *xnodes, double *dxnodes, int restart, char *inputFile, char *outputOptions, species *mix, int num_species) {
  int i;

  N = nodes;
  L_v = Lv;
  nX = numX;
  nX_node = numX_node;
  x = xnodes;
  dx = dxnodes;
  mixture = mix;
  Ns = num_species;
  
  char line[80] = {"dummy"};
  char path[100] = {"./input/"};

  strcat(path,outputOptions);

  //Load flag data from file
  FILE *outParams = fopen(path,"r");

  /*Read file*/
  while (strcmp(line,"Stop") != 0) {
    read_line(outParams,line);

    if(strcmp(line,"density") == 0) 
      densFlag = read_int(outParams);

    if(strcmp(line,"velocity") == 0)
      velFlag = read_int(outParams);

    if(strcmp(line,"temperature") == 0)
      tempFlag = read_int(outParams);

    if(strcmp(line,"pressure") == 0)
      presFlag = read_int(outParams);

    if(strcmp(line,"marginal") == 0)
      marginalFlag = read_int(outParams);

    if(strcmp(line,"slice") == 0)
      sliceFlag = read_int(outParams);

    if(strcmp(line,"entropy") == 0)
      entFlag = read_int(outParams);
  }

  fclose(outParams);

  MPI_Comm_size(MPI_COMM_WORLD,&numNodes);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  if(rank == 0) {

    printf("Opening streams\n");

    fidRho = malloc(Ns*sizeof(FILE *));
    fidKinTemp = malloc(Ns*sizeof(FILE *));
    fidPressure = malloc(Ns*sizeof(FILE *));
    fidBulkV = malloc(Ns*sizeof(FILE *));
    fidSlice = malloc(Ns*sizeof(FILE *));
    fidEntropy = malloc(Ns*sizeof(FILE *));
    fidMarginal = malloc(Ns*sizeof(FILE *));

    //set up dynamically generated filenames + initialize file handles
    char buffer_rho [100];
    char buffer_kintemp [100];
    char buffer_pres [100];
    char buffer_bulkV [100];
    char buffer_slice[100];
    char buffer_entropy[100];
    char buffer_marginal [100];
    
    for(i=0;i<Ns;i++) {
      
      sprintf(buffer_rho,"Data/rho_%s_%s_%s.plt",inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_kintemp,"Data/kintemp_%s_%s_%s.plt",inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_pres,"Data/pres_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_bulkV,"Data/bulkV_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_slice,"Data/slice_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_entropy,"Data/entropy_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
      sprintf(buffer_marginal,"Data/marginal_%s_%s_%s.plt", inputFile, outputOptions, mixture[i].name);
      
      
      if(restart) {
	if(densFlag)
	  fidRho[i] = fopen(buffer_rho, "a");
	
	if(tempFlag)
	  fidKinTemp[i] = fopen(buffer_kintemp, "a");
	
	if(presFlag)
	  fidPressure[i] = fopen(buffer_pres, "a");
	
	if(velFlag)
	  fidBulkV[i] = fopen(buffer_bulkV, "a");
	
	
	if(sliceFlag)
	  fidSlice[i] = fopen(buffer_slice, "a");
	
	if(entFlag)
	  fidEntropy[i] = fopen(buffer_entropy,"a");
	
	if(marginalFlag)
	  fidMarginal[i] = fopen(buffer_marginal, "a");

	
      }
      else {
	if(densFlag)
	  fidRho[i] = fopen(buffer_rho, "w");
	
	if(tempFlag)
	  fidKinTemp[i] = fopen(buffer_kintemp, "w");
	
	if(presFlag)
	  fidPressure[i] = fopen(buffer_pres, "w");
	
	if(velFlag)
	  fidBulkV[i] = fopen(buffer_bulkV, "w");
	
	if(sliceFlag)
	  fidSlice[i] = fopen(buffer_slice, "w");
	
	if(entFlag)
	  fidEntropy[i] = fopen(buffer_entropy, "w");
	
	if(marginalFlag)
	  fidMarginal[i] = fopen(buffer_entropy,"w");
      }
    }
  }
}

void write_streams_inhom(double ***f, double time, double *v, int order) {
  int i,j,k,l,spec;
  double density, kinTemp, bulkV[3], marg, h_v;

  h_v = 2*L_v/N;

  MPI_Status status;
  double momentBuffer[6];

  if(rank == 0) {
    for(spec=0;spec<Ns;spec++) {
      if(densFlag)
	fprintf(fidRho[spec], "ZONE I=%d T=\"T=%g\"\n", nX,time);
      if(tempFlag)
	fprintf(fidKinTemp[spec], "ZONE I=%d T=\"T=%g\"\n", nX,time);
      if(presFlag)
	fprintf(fidPressure[spec], "ZONE I=%d T=\"T=%g\"\n", nX,time);
      if(velFlag)
	fprintf(fidBulkV[spec], "ZONE I=%d T=\"T=%g\"\n", nX,time);
      
      for(l=order;l<(nX_node+order);l++) {
	density = getDensity(f[spec][l],0);
	if(isnan( density)) {
	  printf("nan detected in rank 0 cell %d \n",l);
	  exit(0);
	}
	getBulkVelocity(f[spec][l], bulkV, density,0);
	kinTemp = getTemperature(f[spec][l], bulkV, density,0);
	
	if(densFlag) 
	  fprintf(fidRho[spec], "%le %le\n", x[l], density);
	if(velFlag)
	  fprintf(fidBulkV[spec], "%le %le\n", x[l], bulkV[0]);
	if(tempFlag)
	  fprintf(fidKinTemp[spec], "%le %le\n", x[l], kinTemp);
	if(presFlag)
	  fprintf(fidPressure[spec], "%le %le\n", x[l], getPressure(density, kinTemp));
	if(sliceFlag) {
	  fprintf(fidSlice[spec],    "ZONE I=%d T=\"T=%g X=%g\" \n", N, time, x[l]);
	  for(i=0;i<N;i++)
	    fprintf(fidSlice[spec], "%le %le\n", v[i], f[spec][l][N/2 + N*(N/2 + N*i)]);
	  fprintf(fidSlice[spec],"\n");
	}
	if(marginalFlag) {
	  fprintf(fidMarginal[spec], "ZONE I=%d T=\"T=%g X=%g\" \n", N, time, x[l]);
	  for(i=0;i<N;i++) {
	    marg = 0.0;
	    for(j=0;j<N;j++)
	      for(k=0;k<N;k++) {
		marg += h_v*h_v*f[spec][l][k + N*(j + N*i)];
	      }
	    
	    fprintf(fidMarginal[spec], "%le %le\n", v[i], marg);
	  }
	  fprintf(fidMarginal[spec],"\n");
	}
      }
    
      int nodeCounter;
      for(nodeCounter=1;nodeCounter < numNodes; nodeCounter++) {
	for(l=order;l<nX_node+order;l++) {
	  MPI_Recv(momentBuffer,6,MPI_DOUBLE,nodeCounter,l,MPI_COMM_WORLD,&status);  
	  if(densFlag) 
	    fprintf(fidRho[spec], "%le %le\n", momentBuffer[5], momentBuffer[0]);
	  if(velFlag)
	    fprintf(fidBulkV[spec], "%le %le\n", momentBuffer[5], momentBuffer[1]);
	  if(tempFlag)
	    fprintf(fidKinTemp[spec], "%le %le\n", momentBuffer[5], momentBuffer[4]);
	  if(presFlag)
	    fprintf(fidPressure[spec], "%le %le\n", momentBuffer[5], getPressure(momentBuffer[0], momentBuffer[4]));
	  //if(sliceFlag) {
	  //fprintf(fidSlice, "ZONE I=%d T=\"T=%g X=%g\" \n", N, time, x[l]);
	  //for(i=0;i<N;i++)
	  //fprintf(fidSlice, "%le %le\n", v[i], f[l][N/2 + N*(N/2 + N*i)]);
	  //fprintf(fidSlice,"\n");	
	}
      }
      if(densFlag) 
	fflush(fidRho[spec]);
      if(velFlag)
	fflush(fidBulkV[spec]);
      if(tempFlag)
	fflush(fidKinTemp[spec]);
      if(presFlag)
	fflush(fidPressure[spec]);
      if(marginalFlag)
	fflush(fidMarginal[spec]);
      if(sliceFlag)
	fflush(fidSlice[spec]);
    }
  }
  else {
    for(spec=0;spec<Ns;spec++) {
      for(l=order;l<(nX_node+order);l++) {
	density = getDensity(f[spec][l],0);
	if(isnan( density)) {
	  printf("nan detected in rank %d cell %d\n",rank,l);
	  //exit(0);
	}
	getBulkVelocity(f[spec][l], bulkV, density,0);
	kinTemp = getTemperature(f[spec][l], bulkV, density,0);
	momentBuffer[0] = density;
	momentBuffer[1] = bulkV[0];
	momentBuffer[2] = bulkV[1];
	momentBuffer[3] = bulkV[2];
	momentBuffer[4] = kinTemp;
	momentBuffer[5] = x[l];
	
	MPI_Send(momentBuffer,6,MPI_DOUBLE,0,l,MPI_COMM_WORLD);
      }
    }
  }
}


void close_streams(int homogFlag) {
  int i;

  for(i=0;i<Ns;i++) {
    if(densFlag)
      fclose(fidRho[i]);

    if(tempFlag)
      fclose(fidKinTemp[i]);

    if(presFlag)
      fclose(fidPressure[i]);

    if(velFlag)
      fclose(fidBulkV[i]);

    if(marginalFlag && homogFlag == 1)
      fclose(fidMarginal[i]);

    if(sliceFlag)
      fclose(fidSlice[i]);

    if(entFlag)
      fclose(fidEntropy[i]);

    if(BGKFlag && homogFlag == 0) {
      fclose(fidBGK[i]);
    }
  }
  free(fidRho);
  free(fidKinTemp);
  free(fidPressure);
  free(fidBulkV);
  if(homogFlag)
    free(fidMarginal);
  free(fidEntropy);
  if(!homogFlag) {
    free(fidBGK);
  }
}
