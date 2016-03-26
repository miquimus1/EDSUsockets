#include "comun.h"
#include <unistd.h>
#define DEBUG
int alta_usuario(SOCKADDR_IN *cli_addr);
int baja_usuario(SOCKADDR_IN *cli_addr);
int susc_usuario_tema(SOCKADDR_IN *cli_addr, const char *tema);
int desusc_usuario_tema(SOCKADDR_IN *cli_addr, const char *tema);
int notificar_tema_nuevo(const char *tema);
int notificar_tema_elim(const char *tema);
int notificar_nuevo_evento(const char *tema, const char *valor);
int respuesta(int tipo, int sckt);
int remove_tema(const char *tema);
int buscar_tema(const char *tema);
int remove_usuario_tema(const SOCKADDR_IN *cli_addr, const int index);
int buscar_usuario_tema(const SOCKADDR_IN *cli_addr, const int index);
int remove_usuario(SOCKADDR_IN *cli_addr);
int buscar_usuario(SOCKADDR_IN *cli_addr);
int send_message(TOPIC_MSG *msg, SOCKADDR_IN *cliente);

typedef struct topic
{
  char tp_nam[64];
  int mem_sz;
  SOCKADDR_IN *mem;
} TOPIC;

/* Para no tener que buscar tema por tema a la hora de notificar
   a todos los clientes */
typedef struct suscr
{
  SOCKADDR_IN susc_info;
}SUSCR;

int n_topics = 0, n_suscr = 0, n_avisos = 0;
TOPIC *topics;
SUSCR *suscr;

int main(int argc, char *argv[])
{
  
  if (argc!=3)
    {
      fprintf(stderr, "Uso: %s puerto fichero_temas\n", argv[0]);
      exit(EXIT_FAILURE);
    }

  FILE *file;
  if((file = fopen(argv[2], "r")) == NULL)
    {
      perror("Error");
      exit(EXIT_FAILURE);
    }
  
  topics = malloc(sizeof(TOPIC));
  suscr = malloc(sizeof(SUSCR));
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  int i;
  /* Obtenemos los temas del fichero y inicializamos los struct*/
  while((read = getline(&line, &len, file)) != -1)
    {
      strtok(line, "\n");
      sprintf(topics[n_topics].tp_nam, "%s", line);
      topics[n_topics].mem_sz = 0;
      topics[n_topics++].mem = malloc(sizeof(SOCKADDR_IN));
      topics = realloc(topics, (1+n_topics)*sizeof(TOPIC));
    }
  fclose(file);
  
#ifdef DEBUG /* debug inicialización de temas */
  for(i=0;i<n_topics;i++)
    {
      printf("Topic name:%s , mem_size:%d\n",
	     topics[i].tp_nam, topics[i].mem_sz);
    }
#endif
#ifdef DEBUG
  return 0;
#endif
  
  SOCKADDR_IN serv_addr, cli_addr;
  int sckt_s, sckt, port;
  port = atoi(argv[1]);
  socklen_t addr_sz = sizeof(SOCKADDR_IN);
  
  if((sckt_s = abrir_puerto_escucha(port, &serv_addr))==-1)
    exit(EXIT_FAILURE);

  while(1)
    {
      if((sckt = accept(sckt_s, (SOCKADDR *)&cli_addr, &addr_sz))==-1)
	{
	  perror("Error");
	  exit(EXIT_FAILURE);
	}
      unsigned char buff[4096] = {0};
      ssize_t tam;
      /* unsigned char resp[MAX_REC_SZ]; */
      //metodo 1
      /* while((tam=recv(sckt, (void*)resp, MAX_REC_SZ, 0))>0) */
      /* 	{ */
      /* 	  memcpy(buff + offset, &buf, (size_t)tam); */
      /* 	  offset+=tam; */
      /* 	} */
      //metodo 1
      if((tam=recv(sckt, buff, MAX_REC_SZ, 0))==-1)
	{
	  perror("Error");
	  exit(EXIT_FAILURE);
	}

      TOPIC_MSG *msg;
      msg = deserialize(buff, (size_t)tam);
  
      switch(msg->op)
	{
	case GENEV:
	  notificar_nuevo_evento(msg->tp_nam, msg->tp_val)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case CREAT:
	  notificar_tema_nuevo(msg->tp_nam)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case ELIMT:
	  notificar_tema_elim(msg->tp_nam)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case NEWSC:
	  alta_usuario(&cli_addr)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case FINSC:
	  baja_usuario(&cli_addr)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case ALTAT:
	  susc_usuario_tema(&cli_addr, msg->tp_nam)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);
	  break;
	case BAJAT:
	  desusc_usuario_tema(&cli_addr, msg->tp_nam)==-1? respuesta(ERROR, sckt) : respuesta(OK, sckt);	 
	  break;
	}
      close(sckt);

    }
  
  for(i=0;i<n_topics;i++)
    {
      free(topics[i].mem);
    }
  free(topics);
  free(suscr);
  
  return EXIT_SUCCESS;
}

int respuesta(int tipo, int sckt)
{
  TOPIC_MSG msg;
  bzero((char*)&msg, sizeof(TOPIC_MSG));
  msg.op = tipo;
  
  size_t msg_sz;
  unsigned char *buf = 0;
  msg_sz = serialize(&msg, &buf);
      
  ssize_t tam;
  if((tam=send(sckt, buf, msg_sz, 0))==-1)
    return -1;

  return 0;
  
}


int send_message(TOPIC_MSG *msg, SOCKADDR_IN *cliente)
{
  int sckt;
  if((sckt=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))==-1)
    {
      perror("Error");
      return -1;
    }

  if(connect(sckt, (SOCKADDR*)cliente, sizeof(SOCKADDR_IN))==-1)
    {
      perror("Error");
      return -1;
    }

  size_t msg_sz;
  unsigned char *buf = 0;
  msg_sz = serialize(msg, &buf);
      
  ssize_t tam;
  if((tam=send(sckt, buf, msg_sz, 0))==-1)
    {
      perror("Error");
      exit(EXIT_FAILURE);
    }

  close(sckt);
  return 0;
}

int notificar_nuevo_evento(const char *tema, const char *valor)
{
  int event;
  if((event=buscar_tema(tema))==-1)
    return -1;
  
  TOPIC_MSG msg;
  bzero((char*)&msg, sizeof(TOPIC_MSG));
  msg.op = NOTIF;
  sprintf(msg.tp_nam, "%s", tema);
  sprintf(msg.tp_val, "%s", valor);
  
  int i;
  for(i=0; i<topics[event].mem_sz; i++)
    {
      send_message(&msg, &(topics[event].mem[i]));
    }
  return 0;
}

int notificar_tema_nuevo(const char *tema)
{
  /* Si ya existe error */
  if(buscar_tema(tema)!=-1)
    return -1;

  /* Lo añadimos a la lista de temas */
  sprintf(topics[n_topics].tp_nam, "%s", tema);
  topics[n_topics].mem_sz = 0;
  topics[n_topics++].mem = malloc(sizeof(SOCKADDR_IN));
  topics = realloc(topics, (1+n_topics)*sizeof(TOPIC));

  /* Y notificamos a los suscriptores */
  TOPIC_MSG msg;
  bzero((char*)&msg, sizeof(TOPIC_MSG));
  msg.op = NUEVT;
  sprintf(msg.tp_nam, "%s", tema);

  int i;
  for(i=0; i<n_avisos; i++)
    {
      send_message(&msg, &(suscr[i].susc_info));
    }
  return 0;
}

int notificar_tema_elim(const char *tema)
{
  /* Si falla al eliminar (no existe...) error */
  if(remove_tema(tema)==-1)
    return -1;

  /* Y notificamos */
  TOPIC_MSG msg;
  bzero((char*)&msg, sizeof(TOPIC_MSG));
  msg.op = ELIMT;
  sprintf(msg.tp_nam, "%s", tema);

  int i;
  for(i=0; i<n_avisos; i++)
    {
      send_message(&msg, &(suscr[i].susc_info));
    }
  return 0;
}

int alta_usuario(SOCKADDR_IN *cli_addr)
{
  /* Si ya existe usuario error */
  if(buscar_usuario(cli_addr)!=-1)
    return -1;
  
  /* Lo añadimos a la lista de suscriptores */
  memcpy(&(suscr[n_suscr].susc_info), cli_addr, sizeof(SOCKADDR_IN));
  suscr = realloc(suscr, (1+n_suscr)*sizeof(SUSCR));
  return 0;
}

int baja_usuario(SOCKADDR_IN *cli_addr)
{
  /* Si no existe usuario error */
  if(buscar_usuario(cli_addr)==-1)
    return -1;

  /* Eliminamos usuario de todo */
  if(remove_usuario(cli_addr)==-1)
    return -1;
  
  return 0;
}

int buscar_usuario(SOCKADDR_IN *cli_addr)
{
  int i;

  for(i=0; i<n_suscr; i++)
    {
      if(suscr[i].susc_info.sin_addr.s_addr == cli_addr->sin_addr.s_addr &&
	 suscr[i].susc_info.sin_port == cli_addr->sin_port)
	return i;
    }
  return -1;
}

int remove_usuario(SOCKADDR_IN *cli_addr)
{
  int i;
  for(i=0; i<n_topics; i++)
    {
      remove_usuario_tema(cli_addr, i);
    }

  int elem;
  if((elem=buscar_usuario(cli_addr))==-1)
    return -1;
  
  SUSCR *temp = malloc((n_suscr-1) * sizeof(SUSCR));

  memmove(temp, suscr, (elem+1)*sizeof(SUSCR)); 

  memmove(temp+elem, (suscr)+(elem+1), (n_suscr-elem)*sizeof(SUSCR));

  n_suscr--;
  free(suscr);
  suscr = temp;
  return 0;
}

int susc_usuario_tema(SOCKADDR_IN *cli_addr, const char *tema)
{
  int tema_index;
  /* Tema no existe error */
  if((tema_index=buscar_tema(tema))==-1)
    return -1;

  /* Ya suscrito error */
  if(buscar_usuario_tema(cli_addr, tema_index)!=-1)
    return -1;

  int tam = topics[tema_index].mem_sz;
  /* Lo añadimos a la lista de suscriptores del tema */
  memcpy(&(topics[tema_index].mem[tam]), cli_addr, sizeof(SOCKADDR_IN));
  topics[tema_index].mem_sz++;tam++;
  topics[tema_index].mem = realloc(topics[tema_index].mem,
				   (1+tam)*(sizeof(SOCKADDR_IN)));

  return 0;
}

int desusc_usuario_tema(SOCKADDR_IN *cli_addr, const char *tema)
{
  int tema_index;
  /* Tema no existe error */
  if((tema_index=buscar_tema(tema))==-1)
    return -1;
  
  if(remove_usuario_tema(cli_addr, tema_index)==-1)
    return -1;
  
  return 0;
}

int buscar_usuario_tema(const SOCKADDR_IN *cli_addr, const int index)
{
  int i;
  for(i=0; i<topics[index].mem_sz; i++)
    {
      if(topics[index].mem[i].sin_addr.s_addr == cli_addr->sin_addr.s_addr &&
	 topics[index].mem[i].sin_port == cli_addr->sin_port)
	return i;
    }
  return -1;
}

int remove_usuario_tema(const SOCKADDR_IN *cli_addr, const int index)
{
  int tam = topics[index].mem_sz;
  int elem;
  if((elem=buscar_usuario_tema(cli_addr, index))==-1)
    return -1;
  
  SOCKADDR_IN *temp = malloc((tam-1) * sizeof(SOCKADDR_IN));

  // copiar la parte anterior al tema
  memmove(temp, topics[index].mem, (elem+1)*sizeof(SOCKADDR_IN)); 
  
  // copiar la parte posterior al tema
  memmove(temp+elem, (topics[index].mem)+(elem+1), (tam-elem)*sizeof(SOCKADDR_IN));

  topics[index].mem_sz--;
  free(topics[index].mem);
  topics[index].mem = temp;
  return 0;
}


int buscar_tema(const char *tema)
{
  int i, enc = 0;
  for (i=0; i<n_topics && !enc; i++)
    {
      if(strcmp(topics[i].tp_nam, tema)==0)
	enc = i;
    }
  return enc? enc : -1;
}

int remove_tema(const char *tema)
{
  int elem;
  if((elem=buscar_tema(tema))==-1)
    return -1;
  
  TOPIC *temp = malloc((n_topics-1) * sizeof(TOPIC));

  // copiar la parte anterior al tema
  memmove(temp, topics, (elem+1)*sizeof(TOPIC)); 

  // copiar la parte posterior al tema
  memmove(temp+elem, (topics)+(elem+1), (n_topics-elem)*sizeof(TOPIC));

  n_topics--;
  free(topics);
  topics = temp;
  return 0;
}

