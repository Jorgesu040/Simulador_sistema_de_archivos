#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "cabeceras.h"

#define LONGITUD_COMANDO 100

void Printbytemaps(EXT_BYTE_MAPS *ext_bytemaps);
int ComprobarComando(char *strcomando, char *orden, char *argumento1, char *argumento2);
void LeeSuperBloque(EXT_SIMPLE_SUPERBLOCK *psup);
int BuscaFich(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos,
              char *nombre);
void Directorio(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos);
int Renombrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos,
              char *nombreantiguo, char *nombrenuevo);
int Imprimir(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos,
             EXT_DATOS *memdatos, char *nombre);
int Borrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos,
           EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock,
           char *nombre, FILE *fich);
int Copiar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos,
           EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock,
           EXT_DATOS *memdatos, char *nombreorigen, char *nombredestino, FILE *fich);
void Grabarinodosydirectorio(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, FILE *fich);
void GrabarByteMaps(EXT_BYTE_MAPS *ext_bytemaps, FILE *fich);
void GrabarSuperBloque(EXT_SIMPLE_SUPERBLOCK *ext_superblock, FILE *fich);
void GrabarDatos(EXT_DATOS *memdatos, FILE *fich);

// Funcion que hemos hecho para simplificar el main y separar la logica de procesar los comandos
int ProcesarComando(char *orden, char *argumento1, char *argumento2,
                    EXT_SIMPLE_SUPERBLOCK *ext_superblock, EXT_BYTE_MAPS *ext_bytemaps,
                    EXT_BLQ_INODOS *ext_blq_inodos, EXT_ENTRADA_DIR *directorio,
                    EXT_DATOS *memdatos, FILE *fent, int *grabardatos);

int main()
{

   char comando[LONGITUD_COMANDO];
   char orden[LONGITUD_COMANDO];
   char argumento1[LONGITUD_COMANDO];
   char argumento2[LONGITUD_COMANDO];

   int i, j;
   unsigned long int m;
   EXT_SIMPLE_SUPERBLOCK ext_superblock;
   EXT_BYTE_MAPS ext_bytemaps;
   EXT_BLQ_INODOS ext_blq_inodos;
   EXT_ENTRADA_DIR directorio[MAX_FICHEROS];
   EXT_DATOS memdatos[MAX_BLOQUES_DATOS];
   // Puntero al buffer donde se almacenan los datos leidos
   EXT_DATOS datosfich[MAX_BLOQUES_PARTICION];
   int entradadir;
   int grabardatos;
   FILE *fent;

   int shouldExit = 0;

   // Lectura del fichero completo de una sola /vez

   fent = fopen("particion.bin", "r+b");

   // En donde esta datosfich, lee de SIZE_BLOQUE en SIZE_BLOQUE, hasta un total de MAX_BLOQUES_PARTICION, del fichero fent
   fread(&datosfich, SIZE_BLOQUE, MAX_BLOQUES_PARTICION, fent);

   /* Explicacion operacion de punteros ( struct *)&datosfich[k]
         - datosfich[k] es un bloque de SIZE_BLOQUE bytes
         - &datosfich[k] es el puntero a la primera posicion del bloque
         - (struct *)&datosfich[k] es el puntero a la primera posicion del bloque pero tratado como un puntero a una estructura de tipo struct

      Es decir, cada memcpy copia SIZE_BLOQUE bytes de datosfich[k] a la estructura correspondiente
      Al hacer el casting indicamos el tipo de estructura de los SIZE_BLOQUE bytes que vamos a copiar

      Entonces,
         memcpy(&ext_superblock, (EXT_SIMPLE_SUPERBLOCK *)&datosfich[0], SIZE_BLOQUE);
      es equivalente a
         Donde esta ext_superblock (&ext_superblock) copia SIZE_BLOQUE bytes de donde esta datosfich[0] (&datosfich[0]) haciendo un casting a EXT_SIMPLE_SUPERBLOCK
         pues ext_superblock es de tipo es una estructura de tipo EXT_SIMPLE_SUPERBLOCK
   */
   memcpy(&ext_superblock, (EXT_SIMPLE_SUPERBLOCK *)&datosfich[0], SIZE_BLOQUE);
   memcpy(&directorio, (EXT_ENTRADA_DIR *)&datosfich[3], MAX_FICHEROS * sizeof(EXT_ENTRADA_DIR));
   memcpy(&ext_bytemaps, (EXT_BLQ_INODOS *)&datosfich[1], SIZE_BLOQUE);
   memcpy(&ext_blq_inodos, (EXT_BLQ_INODOS *)&datosfich[2], SIZE_BLOQUE);
   // Resto de memoria
   memcpy(&memdatos, (EXT_DATOS *)&datosfich[4], MAX_BLOQUES_DATOS * SIZE_BLOQUE);

   // Bucle de tratamiento de comandos
   for (;;)
   {
      do
      {
         printf(">> ");
         fflush(stdin);
         fgets(comando, LONGITUD_COMANDO, stdin);
      } while (ComprobarComando(comando, orden, argumento1, argumento2) != 0);

      shouldExit = ProcesarComando(orden, argumento1, argumento2, &ext_superblock, &ext_bytemaps, &ext_blq_inodos, directorio, memdatos, fent, &grabardatos);

      if (grabardatos)
         GrabarDatos(memdatos, fent);

      grabardatos = 0;

      if (shouldExit) {
         fclose(fent);
         break;
      }
   }
}

int ProcesarComando(char *orden, char *argumento1, char *argumento2,
                    EXT_SIMPLE_SUPERBLOCK *ext_superblock, EXT_BYTE_MAPS *ext_bytemaps,
                    EXT_BLQ_INODOS *ext_blq_inodos, EXT_ENTRADA_DIR *directorio,
                    EXT_DATOS *memdatos, FILE *fent, int *grabardatos)
{

   if (strcmp(orden, "info") == 0)
   {
      LeeSuperBloque(ext_superblock);
   }
   else if (strcmp(orden, "bytemaps") == 0)
   {
      Printbytemaps(ext_bytemaps);
   }
   else if (strcmp(orden, "dir") == 0)
   {
      Directorio(directorio, ext_blq_inodos);
   }
   else if (strcmp(orden, "rename") == 0)
   {  
      Renombrar(directorio, ext_blq_inodos, argumento1, argumento2);
      *grabardatos = 1;
      Grabarinodosydirectorio(directorio, ext_blq_inodos, fent);
      GrabarByteMaps(ext_bytemaps, fent);
      GrabarSuperBloque(ext_superblock, fent);
   }
   else if (strcmp(orden, "imprimir") == 0)
   {
      Imprimir(directorio, ext_blq_inodos, memdatos, argumento1);
   }
   else if (strcmp(orden, "remove") == 0)
   {
      Borrar(directorio, ext_blq_inodos, ext_bytemaps, ext_superblock, argumento1, fent);
      *grabardatos = 1;
      Grabarinodosydirectorio(directorio, ext_blq_inodos, fent);
      GrabarByteMaps(ext_bytemaps, fent);
      GrabarSuperBloque(ext_superblock, fent);
   }
   else if (strcmp(orden, "copy") == 0)
   {
      Copiar(directorio, ext_blq_inodos, ext_bytemaps, ext_superblock, memdatos, argumento1, argumento2, fent);
      *grabardatos = 1;
      Grabarinodosydirectorio(directorio, ext_blq_inodos, fent);
      GrabarByteMaps(ext_bytemaps, fent);
      GrabarSuperBloque(ext_superblock, fent);
   }
   else if (strcmp(orden, "salir") == 0)
   {
      return 1;
   }
   else
   {
      printf("Comando desconocido\n");
   }
   return 0;
}

void Printbytemaps(EXT_BYTE_MAPS *ext_bytemaps)
{

   printf("Inodos: \t");
   for (int i = 0; i < MAX_INODOS; i++)
   {
      printf("%d ", ext_bytemaps->bmap_inodos[i]);
   }
   printf("\nBloques [0-25]: ");
   for (int i = 0; i < 25; i++)
   {
      printf("%d ", ext_bytemaps->bmap_bloques[i]);
   }
   putchar('\n');
}

int ComprobarComando(char *strcomando, char *orden, char *argumento1, char *argumento2)
{

   int contadorArgumentos = 0, comandoValido = -1; // -1 indica que el comando no es valido, 0 que es valido
   char *token = NULL;
   char *cadenaTemporal = (char *)malloc(strlen(strcomando) + 1);

   // Reiniciamos las variables de orden, argumento1 y argumento2
   orden[0] = '\0';
   argumento1[0] = '\0';
   argumento2[0] = '\0';

   strcpy(cadenaTemporal, strcomando);
   // Eliminamos el salto de linea
   cadenaTemporal[strlen(cadenaTemporal) - 1] = '\0';

   // Troceamos la cadena
   token = strtok(cadenaTemporal, " ");

   // Si hay mas de 3 argumentos, el comando no es valido
   while (token != NULL)
   {
      // Caso inicial, el primer token es el comando (primera llamada a strtok)
      if (contadorArgumentos == 0)
      {
         strcpy(orden, token);
      }
      else if (contadorArgumentos == 1)
      {
         strcpy(argumento1, token);
      }
      else if (contadorArgumentos == 2)
      {
         strcpy(argumento2, token);
      }
      token = strtok(NULL, " ");
      contadorArgumentos++;
   }

   if (strcmp(orden, "") == 0)
   {
      comandoValido = -1;
   }
   else if (contadorArgumentos > 3 || contadorArgumentos == 0)
   {
      printf("Error: Numero de argumentos incorrecto\n");
   }
   else
   {
      if ((strcmp(orden, "info") == 0 || strcmp(orden, "bytemaps") == 0 || strcmp(orden, "dir") == 0 || strcmp(orden, "salir") == 0) && contadorArgumentos == 1)
      {
         comandoValido = 0;
      }
      else if (strcmp(orden, "rename") == 0 || strcmp(orden, "imprimir") == 0 || strcmp(orden, "remove") == 0 && contadorArgumentos == 2)
      {
         if (strlen(argumento1) > 0)
         {
            comandoValido = 0;
         }
         else
         {
            printf("Error: Falta argumento\n");
         }
      }
      else if (strcmp(orden, "copy") == 0 && contadorArgumentos == 3)
      {
         if (strlen(argumento1) > 0 && strlen(argumento2) > 0)
         {
            comandoValido = 0;
         }
         else
         {
            printf("Error: Faltan argumentos\n");
         }
      }
      else
      {
         printf("Error: Comando no valido\n");
      }
   }

   free(cadenaTemporal);
   return comandoValido;
}

void LeeSuperBloque(EXT_SIMPLE_SUPERBLOCK *psup)
{
   printf("Bloque %d Bytes\n", psup->s_block_size);
   printf("Inodos particion = %d\n", psup->s_inodes_count);
   printf("Inodos libres = %d\n", psup->s_free_inodes_count);
   printf("Bloques particion = %d\n", psup->s_blocks_count);
   printf("Bloques libres = %d\n", psup->s_free_blocks_count);
   printf("Primer bloque de datos = %d\n", psup->s_first_data_block);
}

int BuscaFich(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, char *nombre)
{
   // Implementación de la función para buscar un archivo

   int i, tam;
   for (i = 0; i < MAX_FICHEROS; i++)
   {
      if (directorio[i].dir_inodo != NULL_INODO)
      {
         tam = inodos->blq_inodos[directorio[i].dir_inodo].size_fichero;
         if (tam > 0)
         {
            if (strcmp(nombre, directorio[i].dir_nfich) == 0)
            {
               return i; // se encuentra el fichero
            }
         }
      }
   }

   return -1; // no se encuentra el fichero
}

void Directorio(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos)
{
   for (int i = 0; i < MAX_FICHEROS; i++)
   {
      // Comprueba si el inodo no es nulo y si no es el propio directorio
      if (directorio[i].dir_inodo != NULL_INODO && directorio[i].dir_nfich[0] != '.')
      {

         unsigned int tam = inodos->blq_inodos[directorio[i].dir_inodo].size_fichero;
         if (tam > 0)
         {
            unsigned int numNodo = directorio[i].dir_inodo;
            unsigned short *numBloques = inodos->blq_inodos[numNodo].i_nbloque;
            printf("Nombre: %s\tTamaño: %u\ti-nodo: %u Bloques: [ ", directorio[i].dir_nfich, tam, numNodo);
            for (int j = 0; j < MAX_NUMS_BLOQUE_INODO; j++)
            {
               if (numBloques[j] != NULL_BLOQUE && numBloques[j + 1] != NULL_BLOQUE)
               {
                  printf("%hu, ", numBloques[j]);
               }
               else if (numBloques[j] != NULL_BLOQUE)
               {
                  printf("%hu", numBloques[j]);
               }
            }
            printf(" ] \n");
         }
      }
   }
}

int Renombrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, char *nombreantiguo, char *nombrenuevo)
{
   
   int posicionInodoEnDirectorio;
   int renombradoCorrecto = 0;

   if ((posicionInodoEnDirectorio = BuscaFich(directorio, inodos, nombreantiguo)) == -1) {
      printf("El fichero seleccionado para renombrar no existe\n");
   } else if (strlen(nombrenuevo) > LEN_NFICH - 1) {
      // strlen no cuenta el \0 asi que el maximo es LONGITUD_NFICH - 1
      printf("El nombre del fichero es demasiado largo\n");
      renombradoCorrecto = -1;
   } else {
      strcpy(directorio[posicionInodoEnDirectorio].dir_nfich, nombrenuevo);
      renombradoCorrecto = 1;
      printf("Fichero renombrado correctamente\n");
   }


   return renombradoCorrecto;
}

int Imprimir(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_DATOS *memdatos, char *nombre)
{
   // Implementación de la función para imprimir el contenido de un archivo

   int i, j;
   unsigned long int blnumber;
   EXT_DATOS datosfichero[MAX_NUMS_BLOQUE_INODO];
   i = BuscaFich(directorio, inodos, nombre);
   if (i > 0)
   {
      j = 0;
      do
      {
         blnumber = inodos->blq_inodos[directorio[i].dir_inodo].i_nbloque[j];
         if (blnumber != NULL_BLOQUE)
         {
            datosfichero[j] = memdatos[blnumber - PRIM_BLOQUE_DATOS];
         }
         j++;
      } while ((blnumber != NULL_BLOQUE) && (j < MAX_NUMS_BLOQUE_INODO));
      for (int k = 0; k < j; k++)
      {
         printf("%s", datosfichero[k].dato);
      }
      printf("\n");
   }

   return -2; // no se encuentra el fichero
}

int Borrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock, char *nombre, FILE *fich)
{  
   int posicionInodoEnDirectorio;
   int inodo;

   if ((posicionInodoEnDirectorio = BuscaFich(directorio, inodos, nombre)) == -1) {
      printf("El fichero seleccionado para borrar no existe\n");
   } else {
      // Dentro del directorio coge el que corresponde con el de la iteracion de BuscarFich
      inodo = directorio[posicionInodoEnDirectorio].dir_inodo;

      // De la lista de inodos coge el inodo correspondiente
      EXT_SIMPLE_INODE* ubicacionInodo = &inodos->blq_inodos[inodo];

      // (IMP: posicionInodoEnDirectorio es el indice del inodo en el directorio, inodo es el indice del inodo en la lista de inodos, ubicacionInodo es el inodo en si)

      // Borra toda la informacion de los bloques
      for (int i = 0; i < MAX_NUMS_BLOQUE_INODO; i++){
         int bloqueBorrar = ubicacionInodo->i_nbloque[i];
         if (bloqueBorrar != NULL_BLOQUE) {
            // Marcamos el bloque como no ocupado y aumentamos el numero de no ocupados de manera correspondiente
            ext_bytemaps->bmap_bloques[bloqueBorrar] = 0;
            ext_superblock->s_free_blocks_count++;
            ubicacionInodo->i_nbloque[i] = NULL_BLOQUE;  
         }
      } 

      // Hacemos lo mismo para el inodo correspondiente 
      ext_bytemaps->bmap_inodos[inodo] = 0;
      ext_superblock->s_free_inodes_count++;

      // Borramos el nombre del directorio, el inodo y el tamaño del fichero
      strcpy(directorio[posicionInodoEnDirectorio].dir_nfich, "");
      directorio[posicionInodoEnDirectorio].dir_inodo = NULL_INODO;
      ubicacionInodo->size_fichero = 0;

   }
   return posicionInodoEnDirectorio;
}

int Copiar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock, EXT_DATOS *memdatos, char *nombreorigen, char *nombredestino, FILE *fich)
{
   // Implementación de la función para copiar un archivo
   return 0;
}

void Grabarinodosydirectorio(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, FILE *fich)
{
   // Implementación de la función para grabar inodos y directorio

   fseek(fich, 2 * SIZE_BLOQUE, SEEK_SET);
   fwrite(inodos, 1, SIZE_BLOQUE, fich);
   fseek(fich, 3 * SIZE_BLOQUE, SEEK_SET);
   fwrite(directorio, 1, SIZE_BLOQUE, fich);
}

void GrabarByteMaps(EXT_BYTE_MAPS *ext_bytemaps, FILE *fich)
{
   // Implementación de la función para grabar los mapas de bytes

   fseek(fich, SIZE_BLOQUE, SEEK_SET);
   fwrite(ext_bytemaps, 1, sizeof(EXT_BYTE_MAPS), fich);
}

void GrabarSuperBloque(EXT_SIMPLE_SUPERBLOCK *ext_superblock, FILE *fich)
{
   // Implementación de la función para grabar el superbloque

   fseek(fich, 0, SEEK_SET);
   fwrite(ext_superblock, 1, SIZE_BLOQUE, fich);
}

void GrabarDatos(EXT_DATOS *memdatos, FILE *fich)
{
   // Implementación de la función para grabar los datos

   fseek(fich, 4 * SIZE_BLOQUE, SEEK_SET);
   fwrite(memdatos, MAX_BLOQUES_DATOS, SIZE_BLOQUE, fich);
}