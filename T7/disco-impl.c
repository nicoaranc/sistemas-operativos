/* Necessary includes for device drivers */
#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");

typedef struct pipe {
  char *pipe_buffer;
  int in;
  int out;
  int size;
  KMutex mutex;
  KCondition cond;
  int closed;
} Pipe;

// ... etc.: copie el resto de ../Syncread/syncread-impl.c ...

// * Declaration of disco.c functions */
int disco_open(struct inode *inode, struct file *filp);
int disco_release(struct inode *inode, struct file *filp);
ssize_t disco_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t disco_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void disco_exit(void);
int disco_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations disco_fops = {
  read: disco_read,
  write: disco_write,
  open: disco_open,
  release: disco_release
};

/* Declaration of the init and exit functions */
module_init(disco_init);
module_exit(disco_exit);

/*** El driver para lecturas sincronas *************************************/

#define TRUE 1
#define FALSE 0

/* Global variables of the driver */

int disco_major = 61;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192



static int writing;
static int pendiente;

struct file *pend;

/* El mutex y la condicion para disco */
static KMutex mutex;
static KCondition cond;

int disco_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(disco_major, "disco", &disco_fops);
  if (rc < 0) {
    printk(
      "<1>disco: cannot obtain major number %d\n", disco_major);
    return rc;
  }

  writing= FALSE;
  pendiente= 0;

  m_init(&mutex);
  c_init(&cond);


  printk("<1>Inserting disco module\n");
  return 0;
}

void disco_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(disco_major, "disco");


  printk("<1>Removing disco module\n");
}

int disco_open(struct inode *inode, struct file *filp) {
  int rc= 0;
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    int rc;
    printk("<1>open request for write\n");
    if (pendiente == 0){
      pendiente++;
      printk("<1> Espera lector\n");
      pend = filp;
      while(pendiente){
        if (c_wait(&cond, &mutex)) {
          printk("INTR\n");
          pendiente--;
          // c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }       
      }
      printk("<1> Ya no espera lector\n");
    }
    else{
      pendiente--;
      Pipe *pipe = kmalloc(sizeof(Pipe), GFP_KERNEL);
      if (pipe==NULL) {
        disco_exit();
        return -ENOMEM;
      }
      memset(pipe, 0, sizeof(Pipe));
      pipe->in = pipe->out = pipe->size = pipe->closed = 0;
      m_init(&pipe->mutex);
      c_init(&pipe->cond);
      pipe->pipe_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      if (pipe->pipe_buffer==NULL) {
        disco_exit();
        return -ENOMEM;
      }
      memset(pipe->pipe_buffer, 0, MAX_SIZE);
      filp->private_data = pipe;
      pend->private_data = pipe;
      c_broadcast(&cond);
    }
    printk("<1>open for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    printk("<1>open request for read\n");
    if (pendiente == 0){
      printk("<1> Espera escritor\n");
      pendiente++;
      pend = filp;
      while(pendiente){
        if (c_wait(&cond, &mutex)) {
          pendiente--;
          // c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }       
      }
      printk("<1> Ya no espera escritor\n");
    }
    else{
      pendiente--;
      Pipe *pipe = kmalloc(sizeof(Pipe), GFP_KERNEL);
      if (pipe==NULL) {
        disco_exit();
        return -ENOMEM;
      }
      memset(pipe, 0, sizeof(Pipe));
      pipe->in = pipe->out = pipe->size = pipe->closed = 0;
      m_init(&pipe->mutex);
      c_init(&pipe->cond);
      pipe->pipe_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      if (pipe->pipe_buffer==NULL) {
        disco_exit();
        return -ENOMEM;
      }
      memset(pipe->pipe_buffer, 0, MAX_SIZE);
      filp->private_data = pipe;
      pend->private_data = pipe;
      c_broadcast(&cond);     
      printk("Se hizo la pipe\n");
    }
    printk("<1>open for read\n");
  }

epilog:
  m_unlock(&mutex);
  return rc;
}

int disco_release(struct inode *inode, struct file *filp) {
  m_lock(&mutex);

  if (filp->private_data) {

    Pipe *pipe = (Pipe *)filp->private_data;
    m_lock(&pipe->mutex);
    pipe->closed++;
    c_broadcast(&pipe->cond);
    m_unlock(&pipe->mutex);
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {

    printk("<1>close for read");
  }

  m_unlock(&mutex);
  return 0;
}

ssize_t disco_read(struct file *filp, char *buf,
                    size_t ucount, loff_t *f_pos) {
  int count= ucount;

  printk("<1>read %p %d\n", filp, count);

  Pipe *pipe = (Pipe*)filp->private_data;
  m_lock(&pipe->mutex);


  while (pipe->size==0) {
    /* si no hay nada en el buffer, el lector espera */
    if (c_wait(&pipe->cond, &pipe->mutex)) {
      printk("<1>read interrupted\n");
      count= -EINTR;
      goto epilog;
    }
    if (pipe->closed){
      count = 0;
      goto epilog;
    }
  }

  if (count > pipe->size) {
    count= pipe->size;
  }

  /* Transfiriendo datos hacia el espacio del usuario */
  for (int k= 0; k<count; k++) {
    if (copy_to_user(buf+k, pipe->pipe_buffer+pipe->out, 1)!=0) {
      /* el valor de buf es una direccion invalida */
      count= -EFAULT;
      goto epilog;
    }
    printk("<1>read byte %c (%d) from %d\n",
            pipe->pipe_buffer[pipe->out], pipe->pipe_buffer[pipe->out], pipe->out);
    pipe->out= (pipe->out+1)%MAX_SIZE;
    pipe->size--;
  }

epilog:
  c_broadcast(&pipe->cond);
  m_unlock(&pipe->mutex);
  return count;
}

ssize_t disco_write( struct file *filp, const char *buf,
                      size_t ucount, loff_t *f_pos) {
  int count= ucount;

  printk("<1>write %p %d\n", filp, count);

  Pipe *pipe = filp->private_data;
  m_lock(&pipe->mutex);
  for (int k= 0; k<count; k++) {
    while (pipe->size==MAX_SIZE) {
      /* si el buffer esta lleno, el escritor espera */
      if (c_wait(&pipe->cond, &pipe->mutex)) {
        printk("<1>write interrupted\n");
        count= -EINTR;
        goto epilog;
      }
    }

    if (copy_from_user(pipe->pipe_buffer+pipe->in, buf+k, 1)!=0) {
      /* el valor de buf es una direccion invalida */
      count= -EFAULT;
      goto epilog;
    }
    printk("<1>write byte %c (%d) at %d\n",
          pipe->pipe_buffer[pipe->in], pipe->pipe_buffer[pipe->in], pipe->in);
    pipe->in= (pipe->in+1)%MAX_SIZE;
    pipe->size++;
    c_broadcast(&pipe->cond);
  }

epilog:
  m_unlock(&pipe->mutex); 
  return count;
}