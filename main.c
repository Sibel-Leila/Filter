#include <stdio.h>
#include <mpi.h>

#include "Read.h"

int getnoChild(int **matrix, int n, int rank)
{
	int children = 0, i;

	for (i = 0; i < n; i++) {		
		if(matrix[i][rank] == 1)
			children ++;
	}

	if (rank > 0)
		children --;

	return children;
}

void filterType(int ***filter, char type) {
	// filtru sobel
	if (type == 's') {
		(*filter)[0][0] = 1; (*filter)[0][1] = 0; (*filter)[0][2] = -1;
		(*filter)[1][0] = 2; (*filter)[1][1] = 0; (*filter)[1][2] = -2;
		(*filter)[2][0] = 1; (*filter)[2][1] = 0; (*filter)[2][2] = -1;
	}

	// filtru mean_removal
	if (type == 'm') {
		(*filter)[0][0] = -1; (*filter)[0][1] = -1; (*filter)[0][2] = -1; 
		(*filter)[1][0] = -1; (*filter)[1][1] =  9; (*filter)[1][2] = -1; 		
		(*filter)[2][0] = -1; (*filter)[2][1] = -1; (*filter)[2][2] = -1; 
	}
}

int** filter(int **matrix, int *up, int *down, int n, int m, char type) {
	int **filter, **apply, i, j, k, l;

	// alocam matricea pentru un filtru
	filter = malloc(3 * sizeof(int *));
	for (i = 0; i < 3; i++) 
		filter[i] = malloc (3 * sizeof(int));

	// stabilim filtrul
	filterType(&filter, type);
	
	// alocam matricea noua
	apply = calloc (n, sizeof(int *));
	for (i = 0; i < n; i++)
		apply[i] = calloc(m, sizeof(int));

	// aplicam filtrul
	for (i = 0; i < n; i++)
		for (j = 0; j < m; j++) {
			// pentru fiecare element ii stabilim vecinii
			for (k = i - 1; k < i + 2; k++)
				for (l = j - 1; l < j + 2; l++)
					if (l < 0 || l > m - 1)		// pe coloane
						apply[i][j] += 0;
					else if (k < 0)		// pe liniea de sus
						apply[i][j] += up[l] * filter[k - i + 1][l - j + 1];
					else if (k > n - 1)		// pe linia de jos
						apply[i][j] += down[l] * filter[k - i + 1][l - j + 1];
					else		// interior
						apply[i][j] += matrix[k][l] * filter[k - i + 1][l - j + 1];

			// pentru sobel
			if (type == 's') {
				apply[i][j] += 127;
			}

			// ajustarea noii matrici
			if (apply[i][j] < 0)
				apply[i][j] = 0;

			if (apply[i][j] > 255)
				apply[i][j] = 255;
		}

	free(filter);
	return apply;
}

int main(int argc, char **argv)
{
	int i, j, k, img;
	int **topology;
	int source;
	
	int numImages;
	char *type;
	char **input, **output;

	int **matrix, n, m, **apply;
	int *process, *aux, *all;
	int done, test;

	int rank, myType;
	int noProc;

	int nSrc, nDest, noChild = 0;
	int sizeN;
	int *up, *down;
	int src, dest;

	MPI_Init(&argc, &argv);
	MPI_Status status;
	MPI_Request request;
	
	int children;
	char ping = 'p', recv;
		
	int leaf;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &noProc);

	if (rank == 0) {
		if (argv[1] == NULL)
			printf("Trebuie sa adaugati topologia\n");

		topology = readTopology(argv[1], noProc);

		if(argv[2] == NULL)
			printf("Trebuie sa adaugati lista de imagini\n");

		readListImages(argv[2], &numImages, &type, &input, &output);

		for (i = 0; i < noProc; i++) {
			MPI_Send(&numImages, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		all = calloc(noProc, sizeof(int));

		for (img = 0; img < numImages; img++) {
			 //printf("%d %s %s\n", img, input[img], output[img]);
			
			myType = type[img];

			// trimitem tipul filtrului catre toti
			for (i = 0; i < noProc; i++) 
				MPI_Send(&myType, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);

			// citim imaginea din fisier
			readImage(input[img], &matrix, &m, &n);
			
			// trimitem topologia de la 0 catre toate rankurile
			for (i = 0; i < noProc; i++) {
				for (j = 0; j < noProc; j++)
					for (k = 0; k < noProc; k++)
						MPI_Send(&topology[j][k], 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			}

			//determinam numarul de copii al rootului
			children = getnoChild(topology, noProc, rank);

			// setam marginile de deasupra matricii si dedesubt
			up = calloc(m, sizeof(int));
			down = calloc(m, sizeof(int));

 			process = calloc(noProc, sizeof(int));

			// matricea in care vom memora moua matrice
			apply = calloc(n, sizeof(int*));
			for (i = 0; i < n; i++)
				apply[i] = calloc(m, sizeof(int));
			
			// numarul de copii pe care i-am vizitat
			noChild = 0;

			// trimitem catre copii, imaginea fragmentata
			for (i = 0; i < noProc; i++) {
				if(topology[rank][i] == 1) {
					nSrc = noChild * n / children;
					nDest = nSrc + n / children;
					sizeN = n / children;

					// trimitem un caracter care sa ne ajute la recv
					MPI_Send(&ping, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);

					// trimitem pozitiile de la cat pana la cat este matricea
					MPI_Send(&nSrc, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
					MPI_Send(&nDest, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

					// trimitem inaltimea si latimea matricii
					MPI_Send(&sizeN, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
					MPI_Send(&m, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

					// trimitem un fragment din matrice
					for (j = noChild * sizeN; j < (noChild + 1) * sizeN; j++)
						MPI_Send(&matrix[j][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);
					
					// trimitem linia de deasupra fragmentului nostru din matrice
					k = noChild * sizeN - 1;
					if (k < 0)
						MPI_Send(&up[0], m, MPI_INT, i, 0, MPI_COMM_WORLD);
					else 
						MPI_Send(&matrix[k][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);

					// trimitem linia de dedesuptul fragmentului nostru din matrice
					j = (noChild + 1) * sizeN;
					if (j > n - 1)
						MPI_Send(&down[0], m, MPI_INT, i, 0, MPI_COMM_WORLD);
					else	
						MPI_Send(&matrix[j][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);

					// statistica
					 MPI_Send(&process[0], noProc, MPI_INT, i, 0, MPI_COMM_WORLD);

					/*
					 * DE LA COPII
					 */

					// primim matricea prelucrata
					for (j = noChild * sizeN; j < (noChild + 1) * sizeN; j++)
						MPI_Recv(&apply[j][0], m, MPI_INT, i, 1, MPI_COMM_WORLD, &status);

					aux = malloc(noProc * sizeof(int));
					// primim statistica
					MPI_Recv(&aux[0], noProc, MPI_INT, i, 1, MPI_COMM_WORLD, &status);

					for (j = 0; j < noProc; j++)
						process[j] += aux[j];

					noChild++;
				}
			}
			
			for (i = 0; i < noProc; i++)
				all[i] += process[i];
			
			free(apply);
			free(matrix);
			free(up);
			free(down);
			free(process);
		} 

		// scriem statistica
		writeStat(argv[3], all, noProc);

		free(all);
		free(topology);
/*
 * celelalte rankuri
 */
	} else {
		/*
		 * DE LA PARINTE
		 */

		// primim numarul de imagini care trebuie prelucrate	
		MPI_Recv(&numImages, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

		for (img = 0; img < numImages; img++) {
			// primim tipul filtrului		
			MPI_Recv(&myType, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

			// primim topologia de la 0
			topology = malloc(noProc * sizeof(int *));
			for (i = 0; i < noProc; i++) {
				topology[i] = malloc(noProc * sizeof(int));
			}

			for (j = 0; j < noProc; j++)
				for (k = 0; k < noProc; k++)
					MPI_Recv(&topology[j][k], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
			
			// primim un prim mesaj care ne spune care este parintele nostu, adica sursa
			MPI_Recv(&recv, 1, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

			source = status.MPI_SOURCE;

			// primim care sunt pozitiile de inceput, respectiv sfarsit de la matricea mama si dimensiunea noastra 
			MPI_Recv(&nSrc, 1, MPI_INT, source, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(&nDest, 1, MPI_INT, source, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(&n, 1, MPI_INT, source, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(&m, 1, MPI_INT, source, 0, MPI_COMM_WORLD, &status);

			// primim matricea
			matrix = malloc(n * sizeof(int*));
			for (i = 0; i < n; i++)
				matrix[i] = malloc(m * sizeof(int));

			for (j = 0; j < n; j++)
				MPI_Recv(&matrix[j][0], m, MPI_INT, source, 0, MPI_COMM_WORLD, &status);

			// alocam matricile de deasupra si dedesupt				
			up = calloc(m, sizeof(int));
			down = calloc(m, sizeof(int));

			MPI_Recv(&up[0], m, MPI_INT, source, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(&down[0], m, MPI_INT, source, 0, MPI_COMM_WORLD, &status);

			// primesc statistica
			process = calloc(noProc, sizeof(int));
			MPI_Recv(&process[0], noProc, MPI_INT, source, 0, MPI_COMM_WORLD, &status);

			// matricea cu filtru
			apply = calloc(n, sizeof(int*));
			for (i = 0; i < n; i++)
				apply[i] = calloc(m, sizeof(int));

			// stabilim numarul de copii si tagul cu care vom trimite
			children = getnoChild(topology, noProc, rank);
			noChild = 0;

			// memoram pozitia de inceput a matricii
			src = nSrc;
			dest = nDest;
			
			// vom continua sa impartim imaginea pana nu mai exista frunze
			if (children > 0) {
				for (i = 0; i < noProc; i++) {
					if(topology[rank][i] == 1 && i != source) {
						// stabilim noile limite 
						sizeN = n / children;
						nSrc = src + noChild * sizeN;
						nDest = nSrc + sizeN;

						// trimitem un caracter care sa ne ajute la recv
						MPI_Send(&ping, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);

						// trimitem pozitiile de la cat pana la cat este noua matricea
						MPI_Send(&nSrc, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
						MPI_Send(&nDest, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
						
						// trimitem inaltimea si latimea matricii
						MPI_Send(&sizeN, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
						MPI_Send(&m, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

						// trimitem matricea
						for (j = noChild * sizeN; j < (noChild + 1) * sizeN; j++)
							MPI_Send(&matrix[j][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);

						// stabilim linia de sus si o trimitem
						k = noChild * sizeN - 1;
						if (k < 0)
							MPI_Send(&up[0], m, MPI_INT, i, 0, MPI_COMM_WORLD);
						else 
							MPI_Send(&matrix[k][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);

						// stabilim ultima linie si o trimitem
						j = (noChild + 1) * sizeN;
						if (j > n - 1)
							MPI_Send(&down[0], m, MPI_INT, i, 0, MPI_COMM_WORLD);
						else	
							MPI_Send(&matrix[j][0], m, MPI_INT, i, 0, MPI_COMM_WORLD);

						// statistica
						 MPI_Send(&process[0], noProc, MPI_INT, i, 0, MPI_COMM_WORLD);
		
						// primim matricea prelucrata
						for (j = noChild * sizeN; j < (noChild + 1) * sizeN; j++)
							MPI_Recv(&apply[j][0], m, MPI_INT, i, 1, MPI_COMM_WORLD, &status);

						// primim statistica
						aux = malloc(noProc * sizeof(int));
						MPI_Recv(&aux[0], noProc, MPI_INT, i, 1, MPI_COMM_WORLD, &status);

						for (j = 0; j < noProc; j++)
							process[j] += aux[j];

						noChild++;
					}
				}

				/*
				 * CATRE PARINTE
				 */
				// trimitem catre parinte matricea careia i s-a aplicat filtrul
				for (i = 0; i < n; i++)
					MPI_Send(&apply[i][0], m, MPI_INT, source, 1, MPI_COMM_WORLD);

				MPI_Send(&process[0], noProc, MPI_INT, source, 1, MPI_COMM_WORLD);
			} 

			if (children == 0) {
				// aplicam filtrul
				apply = filter(matrix, up, down, n, m, myType);

				process[rank] = n;

				/*
				 * CATRE PARINTE
				 */
				// trimit maricea
				for (i = 0; i < n; i++) {
					MPI_Send(apply[i], m, MPI_INT, source, 1, MPI_COMM_WORLD);
				}
				//statistica
				MPI_Send(&process[0], noProc, MPI_INT, source, 1, MPI_COMM_WORLD);
			}
			
			free(apply);
			free(topology);
			free(matrix);
			free(up);
			free(down);
		}
	}	

	MPI_Finalize();
	return 0;
}