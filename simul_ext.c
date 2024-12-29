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

   EXT_SIMPLE_SUPERBLOCK ext_superblock;
   EXT_BYTE_MAPS ext_bytemaps;
   EXT_BLQ_INODOS ext_blq_inodos;
   EXT_ENTRADA_DIR directorio[MAX_FICHEROS];
   // Una lista (una estructura) que contiene MAX_BLOQUES_DATOS bloques de datos, cada elemento de la lista tiene un unsigned char dato[SIZE_BLOQUE]
   EXT_DATOS memdatos[MAX_BLOQUES_DATOS];
   // Puntero al buffer donde se almacenan los datos leidos
   EXT_DATOS datosfich[MAX_BLOQUES_PARTICION];
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
   // Este bucle se ejecuta hasta que shouldExit sea 1
   for (;;)
   {
      do
      {
         printf(">> ");
         fflush(stdin);
         fgets(comando, LONGITUD_COMANDO, stdin);

      // Comprobamos si el comando es valido y si no lo es, volvemos a pedirlo
      } while (ComprobarComando(comando, orden, argumento1, argumento2) != 0);

      // Procesamos el comando y si el comando es salir, shouldExit se pone a 1 y se sale del bucle
      shouldExit = ProcesarComando(orden, argumento1, argumento2, &ext_superblock, &ext_bytemaps, &ext_blq_inodos, directorio, memdatos, fent, &grabardatos);

      // Si se ha modificado (marcado por copy, remove o rename) se graban los datos
      if (grabardatos)
         GrabarDatos(memdatos, fent);

      grabardatos = 0;

      if (shouldExit)
      {
         fclose(fent);
         break;
      }
   }
}

// Funcion que procesa los comandos
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
      if(Imprimir(directorio, ext_blq_inodos, memdatos, argumento1) == -1){
         printf("El fichero seleccionado para imprimir no existe\n");
      }
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
      if (Copiar(directorio, ext_blq_inodos, ext_bytemaps, ext_superblock, memdatos, argumento1, argumento2, fent) != -1)
      {
         *grabardatos = 1;
         Grabarinodosydirectorio(directorio, ext_blq_inodos, fent);
         GrabarByteMaps(ext_bytemaps, fent);
         GrabarSuperBloque(ext_superblock, fent);
      }
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

// Muestra por pantalla la ocupación de inodos y los primeros 25 bloques
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

// Divide el comando en orden y hasta dos argumentos
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
      else if (strcmp(orden, "imprimir") == 0 || strcmp(orden, "remove") == 0 && contadorArgumentos == 2)
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
      else if (strcmp(orden, "rename") == 0 || strcmp(orden, "copy") == 0 && contadorArgumentos == 3)
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
         printf("Error: Comando no valido: %s\n (validos: info, bytemaps, dir, rename, imprimir, remove, copy, salir)\n", orden);
      }
   }

   free(cadenaTemporal);
   return comandoValido;
}

// Muestra la información más relevante del superbloque
void LeeSuperBloque(EXT_SIMPLE_SUPERBLOCK *psup)
{
   printf("Bloque %d Bytes\n", psup->s_block_size);
   printf("Inodos particion = %d\n", psup->s_inodes_count);
   printf("Inodos libres = %d\n", psup->s_free_inodes_count);
   printf("Bloques particion = %d\n", psup->s_blocks_count);
   printf("Bloques libres = %d\n", psup->s_free_blocks_count);
   printf("Primer bloque de datos = %d\n", psup->s_first_data_block);
}

// Retorna índice en el array 'directorio' o -1 si no se encuentra
int BuscaFich(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, char *nombre)
{

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

// Muestra la lista de ficheros y sus inodos correspondientes
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

// Comprueba límites de longitud y luego renombra el fichero
int Renombrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, char *nombreantiguo, char *nombrenuevo)
{

   int posicionInodoEnDirectorio;
   int renombradoCorrecto = 0;

   if ((posicionInodoEnDirectorio = BuscaFich(directorio, inodos, nombreantiguo)) == -1)
   {
      printf("El fichero seleccionado para renombrar no existe\n");
   }
   else if (strlen(nombrenuevo) > LEN_NFICH - 1)
   {
      // strlen no cuenta el \0 asi que el maximo es LONGITUD_NFICH - 1
      printf("El nombre del fichero es demasiado largo\n");
      renombradoCorrecto = -1;
   }
   else
   {
      strcpy(directorio[posicionInodoEnDirectorio].dir_nfich, nombrenuevo);
      renombradoCorrecto = 1;
      printf("Fichero renombrado correctamente\n");
   }

   return renombradoCorrecto;
}

 // Permite imprimir fichero desde memdatos (depedencia en BuscaFich)
int Imprimir(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_DATOS *memdatos, char *nombre)
{
   int i, j;
   unsigned long int blnumber;
   EXT_DATOS datosfichero[MAX_NUMS_BLOQUE_INODO];
   i = BuscaFich(directorio, inodos, nombre);
   if (i != -1)
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

   return i; // no se encuentra el fichero si i == -1
}

// Elimina el archivo: libera bloques, inodos y actualiza los byte maps (dependencia en BuscaFich)
int Borrar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock, char *nombre, FILE *fich)
{
   int posicionInodoEnDirectorio;
   int inodo;

   if ((posicionInodoEnDirectorio = BuscaFich(directorio, inodos, nombre)) == -1)
   {
      printf("El fichero seleccionado para borrar no existe\n");
   }
   else
   {
      // Dentro del directorio coge el que corresponde con el de la iteracion de BuscarFich
      inodo = directorio[posicionInodoEnDirectorio].dir_inodo;

      // De la lista de inodos coge el inodo correspondiente
      EXT_SIMPLE_INODE *ubicacionInodo = &inodos->blq_inodos[inodo];

      // (IMP: posicionInodoEnDirectorio es el indice del inodo en el directorio, inodo es el indice del inodo en la lista de inodos, ubicacionInodo es el inodo en si)

      // Borra toda la informacion de los bloques
      for (int i = 0; i < MAX_NUMS_BLOQUE_INODO; i++)
      {
         int bloqueBorrar = ubicacionInodo->i_nbloque[i];
         if (bloqueBorrar != NULL_BLOQUE)
         {
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

// Copia un archivo de origen a destino (dependencia en BuscaFich)
int Copiar(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, EXT_BYTE_MAPS *ext_bytemaps, EXT_SIMPLE_SUPERBLOCK *ext_superblock, EXT_DATOS *memdatos, char *nombreorigen, char *nombredestino, FILE *fich)
{
   /*
   1. Busca fichero origen
   2. Verifica que destino no exista
   3. Reserva inodo destino
   4. Reserva bloques y copia contenido en la estructura de datos: memdatos
   */

   int i, inodo_destino = -1, entrada_directorio_destino = -1;
   int origenCorrecto, destinoCorrecto;
   int copiadoCorrecto = 0;

   // Buscamos el archivo de origen en el directorio
   if ((origenCorrecto = BuscaFich(directorio, inodos, nombreorigen)) == -1)
   {
      printf("Archivo origen no encontrado.\n");
      copiadoCorrecto = -1;
   }

   // Verificamos si el archivo destino ya existe
   if ((destinoCorrecto = BuscaFich(directorio, inodos, nombredestino)) != -1)
   {
      printf("Archivo destino ya existe.\n");
      copiadoCorrecto = -1;
   }

   // Buscamos un espacio libre en el directorio
   i = 0;
   while (i < MAX_INODOS && inodo_destino == -1)
   {
      if (ext_bytemaps->bmap_inodos[i] == 0)
      { // Compare to numeric 0
         inodo_destino = i;
      }
      i++;
   }

   // Mensaje de error si quedan inodos
   if (inodo_destino == -1)
   {
      printf("No hay espacio en el directorio.\n");
      copiadoCorrecto = -1;
   }

   // Comprobacion de la longitud del nombre de destino
   if (strlen(nombredestino) > LEN_NFICH - 1)
   {
      // strlen no cuenta el \0 asi que el maximo es LONGITUD_NFICH - 1
      printf("El nombre del fichero de destino es demasiado largo\n");
      copiadoCorrecto = -1;
   }

   // Busqueda de una entrada libre en el directorio
   i = 0;
   while (i < MAX_FICHEROS && entrada_directorio_destino == -1)
   {
      if (directorio[i].dir_inodo == NULL_INODO)
      {
         entrada_directorio_destino = i;
      }
      i++;
   }
   // Mensaje de error si no hay entradas libres en el directorio
   if (entrada_directorio_destino == -1)
   {
      printf("No hay espacio en el directorio.\n");
      copiadoCorrecto = -1;
   }

   // Proceso de copiado
   if (copiadoCorrecto != -1)
   {
      ext_bytemaps->bmap_inodos[inodo_destino] = 1;
      ext_superblock->s_free_inodes_count--;

      // Copiamos el nombre del archivo
      strcpy(directorio[entrada_directorio_destino].dir_nfich, nombredestino);
      directorio[entrada_directorio_destino].dir_inodo = inodo_destino;

      // Copiamos el tamaño del archivo
      inodos->blq_inodos[inodo_destino].size_fichero = inodos->blq_inodos[directorio[origenCorrecto].dir_inodo].size_fichero;
      unsigned short *numBloquesOrigen = inodos->blq_inodos[directorio[origenCorrecto].dir_inodo].i_nbloque;
      for (int j = 0; j < MAX_NUMS_BLOQUE_INODO; j++)
      {
         if (numBloquesOrigen[j] != NULL_BLOQUE)
         {
            int k = 0;
            int bloqueLibre = -1;
            while (k < MAX_BLOQUES_DATOS && bloqueLibre == -1)
            {
               {
                  if (ext_bytemaps->bmap_bloques[k] == 0)
                  {
                     ext_bytemaps->bmap_bloques[k] = 1;
                     ext_superblock->s_free_blocks_count--;
                     // Añadir el bloque a la lista de bloques del inodo
                     inodos->blq_inodos[inodo_destino].i_nbloque[j] = k;
                     memcpy(&memdatos[k], &memdatos[numBloquesOrigen[j] - PRIM_BLOQUE_DATOS], SIZE_BLOQUE);
                     bloqueLibre = 1;
                  }
                  k++;
               }
            }
         }
      }
      printf("Archivo %s copiado correctamente con el nombre %s\n", nombreorigen, nombredestino);
   }

   return copiadoCorrecto;
}

// Funciones de grabación de los inodos y el directorio en el fichero
void Grabarinodosydirectorio(EXT_ENTRADA_DIR *directorio, EXT_BLQ_INODOS *inodos, FILE *fich)
{
   fseek(fich, 2 * SIZE_BLOQUE, SEEK_SET);
   fwrite(inodos, 1, SIZE_BLOQUE, fich);
   fseek(fich, 3 * SIZE_BLOQUE, SEEK_SET);
   fwrite(directorio, sizeof(EXT_ENTRADA_DIR), MAX_FICHEROS, fich);
}

// Funciones de grabación de los bytemaps en el fichero
void GrabarByteMaps(EXT_BYTE_MAPS *ext_bytemaps, FILE *fich)
{
   fseek(fich, SIZE_BLOQUE, SEEK_SET);
   fwrite(ext_bytemaps, 1, sizeof(EXT_BYTE_MAPS), fich);
}

// Funciones de grabación del superbloque en el fichero
void GrabarSuperBloque(EXT_SIMPLE_SUPERBLOCK *ext_superblock, FILE *fich)
{
   fseek(fich, 0, SEEK_SET);
   fwrite(ext_superblock, 1, SIZE_BLOQUE, fich);
}

// Funciones de grabación de los datos en el fichero
void GrabarDatos(EXT_DATOS *memdatos, FILE *fich)
{
   fseek(fich, 4 * SIZE_BLOQUE, SEEK_SET);
   fwrite(memdatos, MAX_BLOQUES_DATOS, SIZE_BLOQUE, fich);
}