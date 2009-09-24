#define  _GNU_SOURCE   /* Must define this to get access to pthread_rwlock_t */
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <job_queue.h>
#include <msg.h>
#include <util.h>
#include <basic_queue_driver.h>
#include <pthread.h>
#include <unistd.h>
#include <arg_pack.h>

/**
   The running of external jobs is handled thrugh an abstract
   job_queue implemented in this file; the job_queue then contains a
   'driver' which actually runs the job. All drivers must support the
   following functions

     submit: This will submit a job, and return a pointer to a 
             newly allocated queue_job instance.

     clean:  This will clear up all resources used by the job.

     abort:  This will stop the job, and then call clean.

     status: This will get the status of the job. 

     display_info: [Optional] - this is only for the LSF layer to
                   display the name of a possibly faulty node before 
                   restarting.

   When calling the various driver functions the queue layer needs to
   dereference the driver structures, i.e. to get access to the
   driver->submit_jobs function. This is currently (rather clumsily??
   implemented like this):

        When implementing a driver the driver struct MUST start like
        this:
  
        struct some_driver {
            UTIL_TYPE_ID_DECLARATION
            QUEUE_DRIVER_FUNCTIONS
            ....
            ....
        }

        The function allocating a driver instance will just return a
        (void *) however in the queue layer the driver is stored as a
        basic_queue_driver_type instance which is a struct like this:

        struct basic_queue_driver_struct {
            UTIL_TYPE_ID_DECLARATION
            QUEUE_DRIVER_FUNCTIONS
        }   
        
        I.e. it only contains the pointers common to all the driver
        implementations. When calling a driver function the spesific
        driver will cast to it's datatype.

   Observe that this library also contains the files ext_joblist and
   ext_job, those files implement a particular way of dispatching
   external jobs in a series; AFTER THEY HAVE BEEN SUBMITTED. So seen
   from the this scope those files do not provide any particluar
   functionality; there is no compile-time dependencies either (ehhh -
   the EXIT file name is a small link).
*/


/*
  This must match the EXIT_file variable in the job_dispatch script.
*/

#define EXIT_FILE "EXIT"




typedef enum {submit_OK = 0 , submit_job_FAIL , submit_driver_FAIL} submit_status_type;


/**
   This struct holds the job_queue information about one job. Observe
   the following:

    1. This struct is purely static - i.e. it is invisible outside of
       this file-scope.

    2. Typically the driver would like to store some additional
       information, i.e. the PID of the running process for the local
       driver; that is stored in a (driver specific) struct under the
       field job_data.

    3. If the driver detects that a job has failed it leaves an EXIT
       file, the exit status is (currently) not reliably transferred
       back to to the job_queue layer.
       
*/

typedef struct {
  int                  	 external_id;     /* An id number supplied when submitting; this is used when external functions query for status ++ */
  int                  	 submit_attempt;  /* Which attempt is this ... */
  char                  *exit_file;       /* The queue will look for the occurence of this file to detect a failure. */
  char                 	*job_name;        /* The name of the job. */
  char                  *run_path;        /* Where the job is run - absolute path. */
  job_status_type  	 job_status;      /* The current status of the job. */
  basic_queue_job_type 	*job_data;        /* Driver specific data about this job - fully handled by the driver. */
  const void            *job_arg;         /* Untyped data which is sent to the submit function as extra argument - can be whatever - fully owned by external scope.*/
} job_queue_node_type;



/*****************************************************************/

/**
   This is the struct for a whole queue. Observe the following:
   
    1. The number of elements is specified at the allocation time, and
       all nodes are allocated then; i.e. when xx_add_job() is called
       from external scope a new node is not actaully created
       internally, it is just an existing node which is initialized.

    2. The queue can start running before all jobs are added.

*/

struct job_queue_struct {
  int                        active_size;   			/* The number of jobs currently added to the queue. */
  int                        size;          			/* The total number of job slots in the queue. */
  int                        max_submit;    			/* The maximum number of submit attempts for one job. */
  int                        max_running;   			/* The maximum number of concurrently running jobs. */
  char                     * run_cmd;       			/* The command which is run (i.e. path to an executable with arguments). */
  job_queue_node_type     ** jobs;          			/* A vector of job nodes .*/
  basic_queue_driver_type  * driver;        			/* A pointer to a driver instance (LSF|LOCAL|RSH) which actually 'does it'. */
  int                        status_list[JOB_QUEUE_MAX_STATE];  /* The number of jobs in the different states (observe that the state is (ab)used as index). */
  
  unsigned long              usleep_time;                       /* The sleep time before checking for updates. */
  pthread_rwlock_t           active_rwlock;
  pthread_rwlock_t           status_rwlock;                  
};


/*****************************************************************/

static void job_queue_node_clear(job_queue_node_type * node) {
  node->external_id    = -1;
  node->job_status     = JOB_QUEUE_NULL;
  node->submit_attempt = 0;
  node->job_name       = NULL;
  node->run_path       = NULL;
  node->job_data       = NULL;
  node->exit_file      = NULL;
}


static job_queue_node_type * job_queue_node_alloc() {
  job_queue_node_type * node = util_malloc(sizeof * node , __func__);
  job_queue_node_clear(node);
  return node;
}


static void job_queue_node_set_status(job_queue_node_type * node, job_status_type status) {
  node->job_status = status;
}

static void job_queue_node_free_data(job_queue_node_type * node) {
  node->run_path  = util_safe_free(node->run_path);
  node->job_name  = util_safe_free(node->job_name);
  node->exit_file = util_safe_free(node->exit_file);
  if (node->job_data != NULL) 
    util_abort("%s: internal error - driver spesific job data has not been freed - will leak.\n",__func__);
}


static void job_queue_node_free(job_queue_node_type * node) {
  job_queue_node_free_data(node);
  free(node);
}

static job_status_type job_queue_node_get_status(const job_queue_node_type * node) {
  return node->job_status;
}


static int job_queue_node_get_external_id(const job_queue_node_type * node) {
  if (node->external_id < 0) 
    util_abort("%s: tried to get external id from uninitialized job - aborting \n",__func__);
  
  return node->external_id;
}


static void job_queue_node_finalize(job_queue_node_type * node) {
  job_queue_node_free_data(node);
  job_queue_node_clear(node);
}



/*****************************************************************/

static bool job_queue_change_node_status(job_queue_type *  , job_queue_node_type *  , job_status_type );


static void job_queue_initialize_node(job_queue_type * queue , const char * run_path , const char * job_name , int queue_index , int external_id , const void * job_arg) {
  if (external_id < 0) 
    util_abort("%s: external_id must be >= 0 - aborting \n",__func__);
  {
    job_queue_node_type * node = queue->jobs[queue_index];
    node->external_id    = external_id;
    node->submit_attempt = 0;
    node->job_name       = util_alloc_string_copy( job_name );
    node->job_data       = NULL;
    node->job_arg        = job_arg;
    
    if (util_is_abs_path(run_path)) 
      node->run_path = util_alloc_string_copy( run_path );
    else
      node->run_path = util_alloc_realpath( run_path );
    if ( !util_path_exists(node->run_path) ) 
      util_abort("%s: the run_path: %s does not exist - aborting \n",__func__ , node->run_path);

    node->exit_file = util_alloc_filename(node->run_path , EXIT_FILE , NULL);
    job_queue_change_node_status(queue , node , JOB_QUEUE_WAITING);
  }
}


static int job_queue_get_active_size(job_queue_type * queue) {
  int active_size;
  pthread_rwlock_rdlock( &queue->active_rwlock );
  active_size = queue->active_size;
  pthread_rwlock_unlock( &queue->active_rwlock );
  return active_size;
}


static void job_queue_assert_queue_index(const job_queue_type * queue , int queue_index) {
  if (queue_index < 0 || queue_index >= queue->size) 
    util_abort("%s: invalid queue_index - internal error - aborting \n",__func__);
}



static bool job_queue_change_node_status(job_queue_type * queue , job_queue_node_type * node , job_status_type new_status) {
  bool status_change = false;
  pthread_rwlock_wrlock( &queue->status_rwlock );
  {
    job_status_type old_status = job_queue_node_get_status(node);
    job_queue_node_set_status(node , new_status);
    queue->status_list[old_status]--;
    queue->status_list[new_status]++;
    if (new_status != old_status)
      status_change = true;
  }
  pthread_rwlock_unlock( &queue->status_rwlock );
  return status_change;
}


static void job_queue_free_job(job_queue_type * queue , job_queue_node_type * node) {
  basic_queue_driver_type *driver  = queue->driver;
  driver->free_job(driver , node->job_data);
  node->job_data = NULL;
}


static void job_queue_update_status(job_queue_type * queue ) {
  basic_queue_driver_type *driver  = queue->driver;
  int ijob;
  for (ijob = 0; ijob < queue->size; ijob++) {
    job_queue_node_type * node       = queue->jobs[ijob];
    if (node->job_data != NULL) {
      job_status_type  new_status = driver->get_status(driver , node->job_data);
      job_queue_change_node_status(queue , node , new_status);
    }
  }
}



static submit_status_type job_queue_submit_job(job_queue_type * queue , int queue_index) {
  submit_status_type submit_status;
  job_queue_assert_queue_index(queue , queue_index);
  {
    job_queue_node_type     * node    = queue->jobs[queue_index];
    basic_queue_driver_type * driver  = queue->driver;
    
    if (node->submit_attempt < queue->max_submit) {
      {
	basic_queue_job_type * job_data = driver->submit(queue->driver         , 
							 queue_index           , 
							 queue->run_cmd        , 
							 node->run_path        , 
							 node->job_name        , 
							 node->job_arg);
	
	if (job_data != NULL) {
	  job_queue_change_node_status(queue , node , driver->get_status(driver , node->job_data));
	  node->job_data = job_data;
	  node->submit_attempt++;
	  submit_status = submit_OK;
	} else
	  submit_status = submit_driver_FAIL;
      }
    } else {
      job_queue_change_node_status(queue , node , JOB_QUEUE_RUN_FAIL);
      submit_status = submit_job_FAIL;
    }
    return submit_status;
  }
}



static void job_queue_print_status(const job_queue_type * queue) {
  printf("active_size ......: %d \n",queue->active_size);
}

/**
   Will return -1 if no node with the external_id can be found.
*/
static int job_queue_get_internal_index(job_queue_type * queue , int external_id) {
  bool node_found = false;
  int queue_index = 0;
  int active_size = job_queue_get_active_size(queue);
  while (queue_index < active_size) {
    job_queue_node_type * node = queue->jobs[queue_index];
    if (job_queue_node_get_external_id(node) == external_id) {
      node_found = true;
      break;
    } else
      queue_index++;
  }
  if (node_found)
    return queue_index;
  else
    return -1;
}


job_status_type job_queue_export_job_status(job_queue_type * queue , int external_id) {
  int queue_index    = job_queue_get_internal_index( queue , external_id );
  if (queue_index >= 0) {
    job_queue_node_type * node = queue->jobs[queue_index];
    return node->job_status;
  } else {
    job_queue_print_status(queue);
    util_abort("%s: could not find job with id: %d - aborting.\n",__func__ , external_id);
    return 0; 
  }
}



void job_queue_set_load_OK(job_queue_type * queue , int external_id) {
  int queue_index    = job_queue_get_internal_index( queue , external_id );
  if (queue_index >= 0) {
    job_queue_node_type * node = queue->jobs[queue_index];
    job_queue_change_node_status( queue , node , JOB_QUEUE_ALL_OK);
  } else 
    util_abort("%s: could not find job with id:%d - aborting \n",__func__ , external_id);
}


/**
   The external scope asks the queue to restart the the job. We reset
   the submit counter to zero.
*/
   
void job_queue_set_external_restart(job_queue_type * queue , int external_id) {
  int queue_index    = job_queue_get_internal_index( queue , external_id );
  if (queue_index >= 0) {
    job_queue_node_type * node = queue->jobs[queue_index];
    node->submit_attempt = 0;
    job_queue_change_node_status( queue , node , JOB_QUEUE_WAITING);
  } else 
    util_abort("%s: could not find job with id:%d - aborting \n",__func__ , external_id);
}


void job_queue_set_external_fail(job_queue_type * queue , int external_id) {
  int queue_index    = job_queue_get_internal_index( queue , external_id );
  if (queue_index >= 0) {
    job_queue_node_type * node = queue->jobs[queue_index];
    node->submit_attempt = 0;
    job_queue_change_node_status( queue , node , JOB_QUEUE_RUN_FAIL);
  } else 
    util_abort("%s: could not find job with id:%d - aborting \n",__func__ , external_id);
}



static void job_queue_print_jobs(const job_queue_type *queue) {
  int waiting  = queue->status_list[JOB_QUEUE_WAITING];
  int pending  = queue->status_list[JOB_QUEUE_PENDING];

  /* 
     EXIT and DONE are included in "xxx_running", because the target
     file has not yet been checked.
  */
  int running  = queue->status_list[JOB_QUEUE_RUNNING] + queue->status_list[JOB_QUEUE_DONE] + queue->status_list[JOB_QUEUE_EXIT];
  int complete = queue->status_list[JOB_QUEUE_ALL_OK];
  int failed   = queue->status_list[JOB_QUEUE_RUN_FAIL];
  int loading  = queue->status_list[JOB_QUEUE_RUN_OK];  

  printf("Waiting: %3d    Pending: %3d    Running: %3d     Loading: %3d    Failed: %3d   Complete: %3d   [ ]\b",waiting , pending , running , loading , failed , complete);
  fflush(stdout);
}


/** 
    This function goes through all the nodes and call finalize on them. It
    is essential that this routine is not called before all the jobs have
    completed.
*/

void job_queue_finalize(job_queue_type * queue) {
  int i;
  for (i=0; i < queue->size; i++) 
    job_queue_node_finalize(queue->jobs[i]);
  
  for (i=0; i < JOB_QUEUE_MAX_STATE; i++) 
    queue->status_list[i] = 0;
  
  queue->active_size = 0;
}



void job_queue_run_jobs(job_queue_type * queue , int num_total_run) {
  msg_type * submit_msg = msg_alloc("Submitting new jobs:  ]");
  bool new_jobs = false;
  bool cont     = true;
  int  phase = 0;
  int  old_status_list[JOB_QUEUE_MAX_STATE];
  {
    int i;
    for (i=0; i < JOB_QUEUE_MAX_STATE; i++)
      old_status_list[i] = -1;
  }

  
  do {
    char spinner[4];
    spinner[0] = '-';
    spinner[1] = '\\';
    spinner[2] = '|';
    spinner[3] = '/';

    job_queue_update_status(queue);
    if ( (memcmp(old_status_list , queue->status_list , JOB_QUEUE_MAX_STATE * sizeof * old_status_list) != 0) || new_jobs ) {
      printf("\b \n");
      job_queue_print_jobs(queue);
      memcpy(old_status_list , queue->status_list , JOB_QUEUE_MAX_STATE * sizeof * old_status_list);
    } 
    
    if ((queue->status_list[JOB_QUEUE_ALL_OK] + queue->status_list[JOB_QUEUE_RUN_FAIL]) == num_total_run)
      cont = false;
    
    if (cont) {
      printf("\b%c",spinner[phase]); 
      fflush(stdout);
      phase = (phase + 1) % 4;
      
      {
	/* Submitting new jobs */
	
	int active_size    = job_queue_get_active_size(queue);
	int total_active   = queue->status_list[JOB_QUEUE_PENDING] + queue->status_list[JOB_QUEUE_RUNNING];
	int num_submit_new = queue->max_running - total_active; 
	char spinner2[2];
	spinner2[1] = '\0';
	
	new_jobs = false;
	if (queue->status_list[JOB_QUEUE_WAITING] > 0)   /* We have waiting jobs at all           */
	  if (num_submit_new > 0)                        /* The queue can allow more running jobs */
	    new_jobs = true;
	

	if (new_jobs) {
	  int submit_count = 0;
	  int queue_index  = 0;

	  while ((queue_index < active_size) && (num_submit_new > 0)) {
	    job_queue_node_type * node = queue->jobs[queue_index];
	    if (job_queue_node_get_status(node) == JOB_QUEUE_WAITING) {
	      {
		submit_status_type submit_status = job_queue_submit_job(queue , queue_index);
		
		if (submit_status == submit_OK) {
		  if (submit_count == 0) {
		    printf("\b");
		    msg_show(submit_msg);
		    printf("\b\b");
		  }
		  spinner2[0] = spinner[phase];
		  msg_update(submit_msg , spinner2);
		  phase = (phase + 1) % 4;
		  num_submit_new--;
		  submit_count++;
		} else if (submit_status == submit_driver_FAIL)
		  break;
	      }
	    }
	    queue_index++;
	  }
	  
	  if (submit_count > 0) {
	    printf("  "); fflush(stdout);
	    msg_hide(submit_msg);
	    printf(" ]\b"); fflush(stdout);
	  } else 
	    /* 
	       We wanted to - and tried - to submit new jobs; but the
	       driver failed to deliver.
	    */
	    new_jobs = false;
	}
      }

      {
	/*
	  Checking for complete / exited jobs.
	*/
	bool verbose = true;
	int queue_index;
	for (queue_index = 0; queue_index < job_queue_get_active_size(queue); queue_index++) {
	  job_queue_node_type * node = queue->jobs[queue_index];
	  switch (job_queue_node_get_status(node)) {
	  case(JOB_QUEUE_DONE):

	    if (util_file_exists(node->exit_file)) {
	      if (verbose) {
		printf("Restarting: %s | ",node->job_name);
                queue->driver->display_info( queue->driver , node->job_data );
              }
	      job_queue_change_node_status(queue , node , JOB_QUEUE_WAITING);
	    } else 
	      job_queue_change_node_status(queue , node , JOB_QUEUE_RUN_OK);

	    job_queue_free_job(queue , node);
	    break;
	  case(JOB_QUEUE_EXIT):
	    if (verbose) {
	      printf("Restarting: %s | ",node->job_name);
              queue->driver->display_info( queue->driver , node->job_data );
            }
	    job_queue_change_node_status(queue , node , JOB_QUEUE_WAITING);
	    job_queue_free_job(queue , node);
	    break;
	  default:
	    break;
	  }
	}
      }
      
      if (!new_jobs)
	usleep(queue->usleep_time);
      
      /*
	else we have submitted new jobs - and know the status is out
	of sync, no need to wait before a rescan.
      */
    }
  } while ( cont );
  printf("\n");
  msg_free(submit_msg , false);
}


void * job_queue_run_jobs__(void * __arg_pack) {
  arg_pack_type * arg_pack = arg_pack_safe_cast(__arg_pack);
  job_queue_type * queue   = arg_pack_iget_ptr(arg_pack , 0);
  int num_total_run        = arg_pack_iget_int(arg_pack , 1);
  
  job_queue_run_jobs(queue , num_total_run);
  return NULL;
}


void job_queue_add_job(job_queue_type * queue , const char * run_path , const char * job_name , int external_id , const void * job_arg) {
  pthread_rwlock_wrlock( &queue->active_rwlock );
  {
    int active_size  = queue->active_size;
    if (active_size == queue->size) 
      util_abort("%s: queue is already filled up with %d jobs - aborting \n",__func__ , queue->size);
    
    job_queue_initialize_node(queue , run_path , job_name , active_size , external_id , job_arg);
    queue->active_size++;
  }
  pthread_rwlock_unlock( &queue->active_rwlock );
}




/**
   Should (in principle) be possible to change driver on a running system whoaaa.
*/

void job_queue_set_driver(job_queue_type * queue , basic_queue_driver_type * driver) {
  if (queue->driver != NULL)
    driver->free_driver(driver);
  
  queue->driver = driver;
}


job_queue_type * job_queue_alloc(int size , int max_running , int max_submit , 
				 const char    	     * run_cmd      	 , 
				 basic_queue_driver_type * driver) {
  job_queue_type * queue = util_malloc(sizeof * queue , __func__);
  
  queue->usleep_time     = 200000; /* 1/5 second */
  queue->max_running     = max_running;
  queue->max_submit      = max_submit;
  queue->size            = size;
  queue->run_cmd         = util_alloc_string_copy(run_cmd);
  queue->jobs            = util_malloc(size * sizeof * queue->jobs , __func__);
  queue->driver          = NULL;
  {
    int i;
    for (i=0; i < size; i++) 
      queue->jobs[i] = job_queue_node_alloc();

    for (i=0; i < JOB_QUEUE_MAX_STATE; i++)
      queue->status_list[i] = 0;
    
    for (i=0; i < size; i++) 
      queue->status_list[job_queue_node_get_status(queue->jobs[i])]++;
  }
  
  job_queue_set_driver(queue , driver);
  pthread_rwlock_init( &queue->active_rwlock , NULL);
  pthread_rwlock_init( &queue->status_rwlock , NULL);
  queue->active_size = 0;
  return queue;
}







void job_queue_free(job_queue_type * queue) {
  free(queue->run_cmd);
  {
    int i;
    for (i=0; i < queue->size; i++) 
      job_queue_node_free(queue->jobs[i]);
    free(queue->jobs);
  }
  {
    basic_queue_driver_type * driver = queue->driver;
    driver->free_driver(driver);
  }
  free(queue);
  queue = NULL;
}
