/* GSL Engine - Flow module operation engine
 * Copyright (C) 2001 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "gslopmaster.h"

#include "gslcommon.h"
#include "gslopnode.h"
#include "gsloputil.h"
#include "gslopschedule.h"
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <errno.h>


/* --- time stamping (debugging) --- */
#define	ToyprofStamp		struct timeval
#define	toyprof_clock_name()	("Glibc gettimeofday(2)")
#define toyprof_stampinit()	/* nothing */
#define	toyprof_stamp(st)	gettimeofday (&(st), 0)
#define	toyprof_stamp_ticks()	(1000000)
static inline guint64
toyprof_elapsed (ToyprofStamp fstamp,
		 ToyprofStamp lstamp)
{
  guint64 first = fstamp.tv_sec * toyprof_stamp_ticks () + fstamp.tv_usec;
  guint64 last  = lstamp.tv_sec * toyprof_stamp_ticks () + lstamp.tv_usec;
  return last - first;
}


/* --- typedefs & structures --- */
typedef struct _Poll Poll;
struct _Poll
{
  Poll	     *next;
  GslPollFunc poll_func;
  gpointer    data;
  guint       n_fds;
  GslPollFD  *fds;
  GslFreeFunc free_func;
};


/* --- prototypes --- */
static void	master_schedule_discard	(void);


/* --- variables --- */
static gboolean	    master_need_reflow = FALSE;
static gboolean	    master_need_process = FALSE;
static OpNode	   *master_consumer_list = NULL;
const gfloat        gsl_engine_master_zero_block[GSL_STREAM_MAX_VALUES] = { 0, }; /* FIXME */
static Poll	   *master_poll_list = NULL;
static guint        master_n_pollfds = 0;
static guint        master_pollfds_changed = FALSE;
static GslPollFD    master_pollfds[GSL_ENGINE_MAX_POLLFDS];
static OpSchedule  *master_schedule = NULL;


/* --- functions --- */
static void
add_consumer (OpNode *node)
{
  g_return_if_fail (OP_NODE_IS_CONSUMER (node) && node->toplevel_next == NULL && node->integrated);

  node->toplevel_next = master_consumer_list;
  master_consumer_list = node;
}

static void
remove_consumer (OpNode *node)
{
  OpNode *tmp, *last = NULL;

  g_return_if_fail (!OP_NODE_IS_CONSUMER (node) || !node->integrated);
  
  for (tmp = master_consumer_list; tmp; last = tmp, tmp = last->toplevel_next)
    if (tmp == node)
      break;
  g_return_if_fail (tmp != NULL);
  if (last)
    last->toplevel_next = node->toplevel_next;
  else
    master_consumer_list = node->toplevel_next;
  node->toplevel_next = NULL;
}

static void
op_node_disconnect (OpNode *node,
		    guint   istream)
{
  OpNode *src_node = node->inputs[istream].src_node;
  guint ostream = node->inputs[istream].src_stream;
  gboolean was_consumer;

  g_assert (ostream < OP_NODE_N_OSTREAMS (src_node) &&
	    src_node->outputs[ostream].n_outputs > 0);	/* these checks better pass */

  node->inputs[istream].src_node = NULL;
  node->inputs[istream].src_stream = ~0;
  node->module.istreams[istream].connected = FALSE;
  was_consumer = OP_NODE_IS_CONSUMER (src_node);
  src_node->outputs[ostream].n_outputs -= 1;
  src_node->module.ostreams[ostream].connected = src_node->outputs[ostream].n_outputs > 0;
  src_node->output_nodes = gsl_ring_remove (src_node->output_nodes, node);
  /* add to consumer list */
  if (!was_consumer && OP_NODE_IS_CONSUMER (src_node))
    add_consumer (src_node);
}

static void
op_node_disconnect_outputs (OpNode *src_node,
			    OpNode *dest_node)
{
  guint i;

  for (i = 0; i < OP_NODE_N_ISTREAMS (dest_node); i++)
    if (dest_node->inputs[i].src_node == src_node)
      op_node_disconnect (dest_node, i);
}

static void
master_process_job (GslJob *job)
{
  switch (job->job_id)
    {
      OpNode *node, *src_node;
      Poll *poll, *poll_last;
      guint istream, ostream;
      GslFlowJob *fjob;
      gboolean was_consumer;
    case OP_JOB_INTEGRATE:
      node = job->data.node;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "integrate(%p)", node);
      g_return_if_fail (node->integrated == FALSE);
      g_return_if_fail (node->sched_tag == FALSE);
      _gsl_mnl_integrate (node);
      if (OP_NODE_IS_CONSUMER (node))
	add_consumer (node);
      node->counter = 0;
      master_need_reflow |= TRUE;
      break;
    case OP_JOB_DISCARD:
      node = job->data.node;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "discard(%p)", node);
      g_return_if_fail (node->integrated == TRUE);
      /* disconnect inputs */
      for (istream = 0; istream < OP_NODE_N_ISTREAMS (node); istream++)
	if (node->inputs[istream].src_node)
	  op_node_disconnect (node, istream);
      /* disconnect outputs */
      while (node->output_nodes)
	op_node_disconnect_outputs (node, node->output_nodes->data);
      /* remove from consumer list */
      if (OP_NODE_IS_CONSUMER (node))
	{
	  _gsl_mnl_remove (node);
	  remove_consumer (node);
	}
      else
	_gsl_mnl_remove (node);
      node->counter = 0;
      master_need_reflow |= TRUE;
      master_schedule_discard ();	/* discard schedule so node may be freed */
      break;
    case GSL_JOB_SET_CONSUMER:
    case GSL_JOB_UNSET_CONSUMER:
      node = job->data.node;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "toggle_consumer(%p)", node);
      was_consumer = OP_NODE_IS_CONSUMER (node);
      node->is_consumer = job->job_id == GSL_JOB_SET_CONSUMER;
      if (was_consumer != OP_NODE_IS_CONSUMER (node))
	{
	  if (OP_NODE_IS_CONSUMER (node))
	    add_consumer (node);
	  else
	    remove_consumer (node);
	  master_need_reflow |= TRUE;
	}
      break;
    case OP_JOB_CONNECT:
      node = job->data.connection.dest_node;
      src_node = job->data.connection.src_node;
      istream = job->data.connection.dest_istream;
      ostream = job->data.connection.src_ostream;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "connect(%p,%u,%p,%u)", node, istream, src_node, ostream);
      g_return_if_fail (node->integrated == TRUE);
      g_return_if_fail (src_node->integrated == TRUE);
      g_return_if_fail (node->inputs[istream].src_node == NULL);
      node->inputs[istream].src_node = src_node;
      node->inputs[istream].src_stream = ostream;
      node->module.istreams[istream].connected = TRUE;
      /* remove from consumer list */
      was_consumer = OP_NODE_IS_CONSUMER (src_node);
      src_node->outputs[ostream].n_outputs += 1;
      src_node->module.ostreams[ostream].connected = TRUE;
      src_node->output_nodes = gsl_ring_append (src_node->output_nodes, node);
      src_node->counter = 0;
      if (was_consumer && !OP_NODE_IS_CONSUMER (src_node))
	remove_consumer (src_node);
      master_need_reflow |= TRUE;
      break;
    case OP_JOB_DISCONNECT:
      node = job->data.connection.dest_node;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "disconnect(%p,%u)", node, job->data.connection.dest_istream);
      g_return_if_fail (node->integrated == TRUE);
      g_return_if_fail (node->inputs[job->data.connection.dest_istream].src_node != NULL);
      op_node_disconnect (node, job->data.connection.dest_istream);
      master_need_reflow |= TRUE;
      break;
    case GSL_JOB_ACCESS:
      node = job->data.access.node;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "access node(%p): %p(%p)", node, job->data.access.access_func, job->data.access.data);
      g_return_if_fail (node->integrated == TRUE);
      job->data.access.access_func (&node->module, job->data.access.data);
      break;
    case GSL_JOB_FLOW_JOB:
      node = job->data.flow_job.node;
      fjob = job->data.flow_job.fjob;
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "add flow_job(%p,%p)", node, fjob);
      g_return_if_fail (node->integrated == TRUE);
      job->data.flow_job.fjob = NULL;	/* ownership taken over */
      fjob->any.next = node->flow_jobs;
      node->flow_jobs = fjob;
      _gsl_mnl_reorder (node);
      break;
    case OP_JOB_DEBUG:
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "debug");
      g_printerr ("JOB-DEBUG: %s\n", job->data.debug);
      break;
    case OP_JOB_ADD_POLL:
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "add poll %p(%p,%u)", job->data.poll.poll_func, job->data.poll.data, job->data.poll.n_fds);
      if (job->data.poll.n_fds + master_n_pollfds > GSL_ENGINE_MAX_POLLFDS)
	g_error ("adding poll job exceeds maximum number of poll-fds (%u > %u)",
		 job->data.poll.n_fds + master_n_pollfds, GSL_ENGINE_MAX_POLLFDS);
      poll = gsl_new_struct0 (Poll, 1);
      poll->poll_func = job->data.poll.poll_func;
      poll->data = job->data.poll.data;
      poll->free_func = job->data.poll.free_func;
      job->data.poll.free_func = NULL;		/* don't free data this round */
      poll->n_fds = job->data.poll.n_fds;
      poll->fds = poll->n_fds ? master_pollfds + master_n_pollfds : master_pollfds;
      master_n_pollfds += poll->n_fds;
      if (poll->n_fds)
	master_pollfds_changed = TRUE;
      memcpy (poll->fds, job->data.poll.fds, sizeof (poll->fds[0]) * poll->n_fds);
      poll->next = master_poll_list;
      master_poll_list = poll;
      break;
    case OP_JOB_REMOVE_POLL:
      OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "remove poll %p(%p)", job->data.poll.poll_func, job->data.poll.data);
      for (poll = master_poll_list, poll_last = NULL; poll; poll_last = poll, poll = poll_last->next)
	if (poll->poll_func == job->data.poll.poll_func && poll->data == job->data.poll.data)
	  {
	    if (poll_last)
	      poll_last->next = poll->next;
	    else
	      master_poll_list = poll->next;
	    break;
	  }
      if (poll)
	{
	  job->data.poll.free_func = poll->free_func;	/* free data with job */
	  poll_last = poll;
	  if (poll_last->n_fds)
	    {
	      for (poll = master_poll_list; poll; poll = poll->next)
		if (poll->fds > poll_last->fds)
		  poll->fds -= poll_last->n_fds;
	      g_memmove (poll_last->fds, poll_last->fds + poll_last->n_fds,
			 ((guint8*) (master_pollfds + master_n_pollfds)) -
			 ((guint8*) (poll_last->fds + poll_last->n_fds)));
	      master_n_pollfds -= poll_last->n_fds;
	      master_pollfds_changed = TRUE;
	    }
	  gsl_delete_struct (Poll, poll_last);
	}
      else
	g_warning (G_STRLOC ": failed to remove unknown poll function %p(%p)",
		   job->data.poll.poll_func, job->data.poll.data);
      break;
    default:
      g_assert_not_reached ();
    }
  OP_DEBUG (GSL_ENGINE_DEBUG_JOBS, "done");
}

static void
master_poll_check (glong   *timeout_p,
		   gboolean check_with_revents)
{
  gboolean need_processing = FALSE;
  Poll *poll;

  if (master_need_process || *timeout_p == 0)
    {
      master_need_process = TRUE;
      return;
    }
  for (poll = master_poll_list; poll; poll = poll->next)
    {
      glong timeout = -1;

      if (poll->poll_func (poll->data, gsl_engine_block_size (), &timeout,
			   poll->n_fds, poll->n_fds ? poll->fds : NULL, check_with_revents)
	  || timeout == 0)
	{
	  need_processing |= TRUE;
	  *timeout_p = 0;
	  break;
	}
      else if (timeout > 0)
	*timeout_p = *timeout_p < 0 ? timeout : MIN (*timeout_p, timeout);
    }
  master_need_process = need_processing;
}

static void
master_process_locked_node (OpNode *node,
			    guint   n_values)
{
  guint i;
  guint64 new_counter = GSL_TICK_STAMP + n_values;

  for (i = 0; i < OP_NODE_N_ISTREAMS (node); i++)
    {
      OpNode *inode = node->inputs[i].src_node;

      if (inode)
	{
	  OP_NODE_LOCK (inode);
	  if (inode->counter < new_counter)
	    master_process_locked_node (inode, new_counter - node->counter);
	  node->module.istreams[i].values = inode->module.ostreams[node->inputs[i].src_stream].values;
	  OP_NODE_UNLOCK (inode);
	}
      else
	node->module.istreams[i].values = gsl_engine_master_zero_block;
    }
  for (i = 0; i < OP_NODE_N_OSTREAMS (node); i++)
    {
      node->module.ostreams[i].values = node->outputs[i].buffer;
      if (node->module.ostreams[i].zero_initialize)
	memset (node->module.ostreams[i].values, 0, gsl_engine_block_size () * sizeof (gfloat));
    }
  node->module.klass->process (&node->module, n_values);
  node->counter += n_values;
}

static GslLong gsl_trace_delay = 0;

static void
master_process_flow (void)
{
  guint64 new_counter = GSL_TICK_STAMP + gsl_engine_block_size ();
  GslLong trace_slowest = 0;
  GslLong trace_delay = gsl_trace_delay;
  OpNode *trace_node = NULL;
  
  g_return_if_fail (master_need_process == TRUE);

  OP_DEBUG (GSL_ENGINE_DEBUG_MASTER, "process_flow");
  if (master_schedule)
    {
      OpNode *node;

      _op_schedule_restart (master_schedule);
      _gsl_com_set_schedule (master_schedule);
      
      node = _gsl_com_pop_unprocessed_node ();
      while (node)
	{
	  ToyprofStamp trace_stamp1, trace_stamp2;

	  if_reject (trace_delay)
	    toyprof_stamp (trace_stamp1);
	  
	  master_process_locked_node (node, gsl_engine_block_size ());
	  
	  if_reject (trace_delay)
	    {
	      GslLong duration;

	      toyprof_stamp (trace_stamp2);
	      duration = toyprof_elapsed (trace_stamp1, trace_stamp2);
	      if (duration > trace_slowest)
		{
		  trace_slowest = duration;
		  trace_node = node;
		}
	    }
	  
	  _gsl_com_push_processed_node (node);
	  node = _gsl_com_pop_unprocessed_node ();
	}

      if_reject (trace_delay)
	{
	  if (trace_node)
	    {
	      if (trace_slowest > trace_delay)
		g_print ("Excess Node: %p  Duration: %lu usecs     ((void(*)())%p)         \n",
			 trace_node, trace_slowest, trace_node->module.klass->process);
	      else
		g_print ("Slowest Node: %p  Duration: %lu usecs     ((void(*)())%p)         \r",
			 trace_node, trace_slowest, trace_node->module.klass->process);
	    }
	}

      /* walk unscheduled nodes which have flow jobs */
      node = _gsl_mnl_head ();
      while (node && GSL_MNL_HEAD_NODE (node))
	{
	  OpNode *tmp = node->mnl_next;
	  GslFlowJob *fjob = _gsl_node_pop_flow_job (node, new_counter);

	  if (fjob)
	    {
	      while (fjob)
		{
		  g_printerr ("ignoring flow_job %p\n", fjob);
		  fjob = _gsl_node_pop_flow_job (node, new_counter);
		}
	      _gsl_mnl_reorder (node);
	    }
	  node = tmp;
	}

      /* nothing new to process, wait on slaves */
      _gsl_com_wait_on_unprocessed ();

      _gsl_com_unset_schedule (master_schedule);
      _gsl_tick_stamp_inc ();
      _gsl_recycle_const_values ();
    }
  master_need_process = FALSE;
}

static void
master_reschedule_flow (void)
{
  OpNode *node;

  g_return_if_fail (master_need_reflow == TRUE);

  OP_DEBUG (GSL_ENGINE_DEBUG_MASTER, "flow_reschedule");
  if (!master_schedule)
    master_schedule = _op_schedule_new ();
  else
    {
      _op_schedule_unsecure (master_schedule);
      _op_schedule_clear (master_schedule);
    }
  for (node = master_consumer_list; node; node = node->toplevel_next)
    _op_schedule_consumer_node (master_schedule, node);
  _op_schedule_secure (master_schedule);
  master_need_reflow = FALSE;
}

static void
master_schedule_discard (void)
{
  g_return_if_fail (master_need_reflow == TRUE);

  if (master_schedule)
    {
      _op_schedule_unsecure (master_schedule);
      _op_schedule_destroy (master_schedule);
      master_schedule = NULL;
    }
}


/* --- MasterThread main loop --- */
gboolean
_gsl_master_prepare (GslEngineLoop *loop)
{
  gboolean need_dispatch;
  guint i;

  g_return_val_if_fail (loop != NULL, FALSE);

  /* setup and clear pollfds here already, so master_poll_check() gets no junk (and IRIX can't handle non-0 revents) */
  loop->fds_changed = master_pollfds_changed;
  master_pollfds_changed = FALSE;
  loop->n_fds = master_n_pollfds;
  loop->fds = master_pollfds;
  for (i = 0; i < loop->n_fds; i++)
    loop->fds[i].revents = 0;
  loop->revents_filled = FALSE;

  loop->timeout = -1;
  /* cached checks first */
  need_dispatch = master_need_reflow || master_need_process;
  /* lengthy query */
  if (!need_dispatch)
    need_dispatch = op_com_job_pending ();
  /* invoke custom poll checks */
  if (!need_dispatch)
    {
      master_poll_check (&loop->timeout, FALSE);
      need_dispatch = master_need_process;
    }
  if (need_dispatch)
    loop->timeout = 0;

  OP_DEBUG (GSL_ENGINE_DEBUG_MASTER, "PREPARE: need_dispatch=%u timeout=%6ld n_fds=%u",
	    need_dispatch,
	    loop->timeout, loop->n_fds);

  return need_dispatch;
}

gboolean
_gsl_master_check (const GslEngineLoop *loop)
{
  gboolean need_dispatch;

  g_return_val_if_fail (loop != NULL, FALSE);
  g_return_val_if_fail (loop->n_fds == master_n_pollfds, FALSE);
  g_return_val_if_fail (loop->fds == master_pollfds, FALSE);
  if (loop->n_fds)
    g_return_val_if_fail (loop->revents_filled == TRUE, FALSE);

  /* cached checks first */
  need_dispatch = master_need_reflow || master_need_process;
  /* lengthy query */
  if (!need_dispatch)
    need_dispatch = op_com_job_pending ();
  /* invoke custom poll checks */
  if (!need_dispatch)
    {
      glong dummy = -1;

      master_poll_check (&dummy, TRUE);
      need_dispatch = master_need_process;
    }

  OP_DEBUG (GSL_ENGINE_DEBUG_MASTER, "CHECK: need_dispatch=%u", need_dispatch);

  return need_dispatch;
}

void
_gsl_master_dispatch_jobs (void)
{
  GslJob *job;

  job = gsl_com_pop_job ();
  while (job)
    {
      master_process_job (job);
      job = gsl_com_pop_job ();	/* have to process _all_ jobs */
    }
}

void
_gsl_master_dispatch (void)
{
  /* processing has prime priority, but we can't process the
   * network, until all jobs have been handled and if necessary
   * rescheduled the network.
   * that's why we have to handle everything at once and can't
   * preliminarily return after just handling jobs or rescheduling.
   */
  _gsl_master_dispatch_jobs ();
  if (master_need_reflow)
    master_reschedule_flow ();
  if (master_need_process)
    master_process_flow ();
}

void
_gsl_master_thread (gpointer data)
{
  gboolean run = TRUE;

  /* assert sane configuration checks, since we're simply casting structures */
  g_assert (sizeof (struct pollfd) == sizeof (GslPollFD) &&
	    G_STRUCT_OFFSET (GslPollFD, fd) == G_STRUCT_OFFSET (struct pollfd, fd) &&
	    G_STRUCT_OFFSET (GslPollFD, events) == G_STRUCT_OFFSET (struct pollfd, events) &&
	    G_STRUCT_OFFSET (GslPollFD, revents) == G_STRUCT_OFFSET (struct pollfd, revents));

  /* add the thread wakeup pipe to master pollfds, so we get woken
   * up in time (even though we evaluate the pipe contents later)
   */
  gsl_thread_get_pollfd (master_pollfds);
  master_n_pollfds += 1;
  master_pollfds_changed = TRUE;

  toyprof_stampinit ();
  
  while (run)
    {
      GslEngineLoop loop;
      gboolean need_dispatch;

      need_dispatch = _gsl_master_prepare (&loop);

      if (!need_dispatch)
	{
	  gint err;

	  err = poll ((struct pollfd*) loop.fds, loop.n_fds, loop.timeout);
	  
	  if (err >= 0)
	    loop.revents_filled = TRUE;
	  else
	    g_printerr (G_STRLOC ": poll() error: %s\n", g_strerror (errno));

	  if (loop.revents_filled)
	    need_dispatch = _gsl_master_check (&loop);
	}

      if (need_dispatch)
	_gsl_master_dispatch ();

      /* handle thread pollfd messages */
      run = gsl_thread_sleep (0);
    }
}
