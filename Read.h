#include <stdlib.h>
#include <string.h>

int** readTopology(char *fileName, int noTop)
{
	int i, j;
	FILE *fp;
	int src, dest;
	char *line = malloc(300 * sizeof(char));
	int **topology;

	topology = calloc(noTop, sizeof(int *));

	for (i = 0; i < noTop; i++)
		topology[i] = calloc(noTop, sizeof(int));

	fp = fopen(fileName , "r");
	
	if (fp == NULL) {
		printf("Error opening input file!\n");
		exit(-1);
	}

	while (fgets(line, 300, fp) != NULL) {
		// transform primul element in intreg, gasind astfel parintele
		src = atoi(line);

		// pentru fiecare numar de pe linia respectiva determin destinatia
		for (i = 3; i < strlen(line); i++) {
			dest = atoi(line + i);

			// setez matricea
			topology[src][dest] = 1;

			// parcurg fiecare cifra pana la intalnirea lui "blank"
			// ca apoi sa determin urmatoarea destinatie
			while (line[i] != ' ')
				i++;
		}
	}

	fclose(fp);

	free(line);

	return topology;
}

void readListImages(char *fileName, int *num, char **type, char ***input, char ***output)
{
	FILE *fp;
	int i;
	char *line = malloc(300 * sizeof(char));
	char *typeF, *inputF, *outputF;

	int numImages;
	typeF = malloc(15 * sizeof(char));
	inputF = malloc(30 * sizeof(char));
	outputF = malloc(30 * sizeof(char));

	fp = fopen(fileName , "r");
	
	if (fp == NULL) {
		printf("Error opening input file!\n");
		exit(-1);
	}

	// citesc numarul de imaginii
	fgets(line, 300, fp);
	numImages = atoi(line);

	(*num) = numImages;

	(*type) = malloc(numImages * sizeof(char));
	(*input) = malloc(numImages * sizeof(char *));
	(*output) = malloc(numImages * sizeof(char *));

	for (i = 0; i < numImages; i++) {
		// citesc fiecare linie si determin tipul de filtru, fisierul IN si fisierul OUT
		fgets(line, 300, fp);
		sscanf(line, "%s %s %s", typeF, inputF, outputF);

		// daca este sobel setez s, respectiv m daca este mean removal; deoarece types este un vector de caractere
		if (strcmp(typeF, "sobel") == 0)
			(*type)[i] = 's';

		if (strcmp(typeF, "mean_removal") == 0)
			(*type)[i] = 'm';

		// copiez fisierul de input
		(*input)[i] = malloc(strlen(inputF) * sizeof(char));
		strcpy((*input)[i], inputF);

		// copiez fisierul de output
		(*output)[i] = malloc(strlen(outputF) * sizeof(char));
		strcpy((*output)[i], outputF);
	}

	fclose(fp);

	free(line);
	free(typeF);
	free(inputF);
	free(outputF);
}

void readImage(char *fileName, int ***matrix, int *m, int *n)
{
	FILE *fp;
	int i, j;
	int value;
	char *line = malloc(300 * sizeof(char));

	fp = fopen(fileName , "r");
	
	if (fp == NULL) {
		printf("Error opening input file!\n");
		exit(-1);
	}

	// magicNumber
	fgets(line, 300, fp);

	// comment
	fgets(line, 300, fp);
	
	// citesc dimensiunile
	fgets(line, 300, fp);
	(*m) = atoi(line);
	

	for (i = 0; line[i] != ' '; i++);
	
	(*n) = atoi(line + i);

	// size
	fgets(line, 300, fp);

	// citesc matricea de pixeli 
	(*matrix) = malloc((*n) * sizeof(int*));
	for (i = 0; i < (*n); i++)
		(*matrix)[i] = malloc((*m) * sizeof(int));

	for (i = 0; i < (*n); i++) {
		for (j = 0; j < (*m); j++) {
			fgets(line, 30, fp);		
			(*matrix)[i][j] = atoi(line);
		}
	}
	
	fclose(fp);

	free(line);
}

void writeImage(char *fileNameOut, char *fileNameIn, int **apply, int m, int n)
{

	FILE *fpi;
	char *magic = malloc(300 * sizeof(char));
	char *comment = malloc(300 * sizeof(char));

	fpi = fopen(fileNameIn , "r");
	
	if (fpi == NULL) {
		printf("Error opening input file!\n");
		exit(-1);
	}

	fgets(magic, 300, fpi);	// magicNumber
	fgets(comment, 300, fpi);	// comment

	fclose(fpi);

	FILE *fp;
	int i, j;
	char *line = malloc(300 * sizeof(char));

	fp = fopen(fileNameOut , "w");
	
	if (fp == NULL) {
		printf("Error opening output file!\n");
		exit(-1);
	}

	fprintf(fp, "%s", magic);	// magicNumber
	fprintf(fp, "%s", comment);	// comment
	
	fprintf(fp, "%d %d\n", m, n);	// size
	fprintf(fp, "255\n");	//max size
	
	for (i = 0; i < n; i++) {
		for (j = 0; j < m; j++) {
			fprintf(fp, "%d\n", apply[i][j]);
		}
	}

	fclose(fp);
	free(comment);
	free(magic);
}

void writeStat(char *fileName, int *process, int n) {
	FILE *fp;
	int i;

	fp = fopen(fileName , "w");
	
	if (fp == NULL) {
		printf("Error opening input file!\n");
		exit(-1);
	}

	for (i = 0; i < n; i++)
		fprintf(fp, "%d: %d\n", i, process[i]);

	fclose(fp);
}