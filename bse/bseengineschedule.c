/* GSL Engine - Flow module operation engine
 * Copyright (C) 2001, 2002 Tim Janik
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include "gslopschedule.h"


#include "gslcommon.h"


/* --- prototypes --- */
static void	schedule_node		(EngineSchedule	*schedule,
					 EngineNode	*node,
					 guint		 leaf_level);
static void	schedule_cycle		(EngineSchedule	*schedule,
					 GslRing	*cycle_nodes,
					 guint		 leaf_level);
static void 	subschedule_query_node	(EngineSchedule *schedule,
					 EngineNode     *node,
					 EngineQuery    *query);


/* --- functions --- */
EngineSchedule*
_engine_schedule_new (void)
{
  EngineSchedule *sched = gsl_new_struct0 (EngineSchedule, 1);
  
  sched->n_items = 0;
  sched->leaf_levels = 0;
  sched->nodes = NULL;
  sched->cycles = NULL;
  sched->secured = FALSE;
  sched->in_pqueue = FALSE;
  sched->cur_leaf_level = ~0;
  sched->cur_node = NULL;
  sched->cur_cycle = NULL;
  sched->vnodes = NULL;

  return sched;
}

static inline void
unschedule_virtual (EngineSchedule *sched,
		    EngineNode     *vnode)
{
  g_return_if_fail (ENGINE_NODE_IS_SCHEDULED (vnode) == TRUE);
  g_return_if_fail (sched->n_items > 0);

  /* SCHED_DEBUG ("unschedule_virtual(%p)", vnode); */
  sched->vnodes = gsl_ring_remove (sched->vnodes, vnode);
  vnode->sched_tag = FALSE;
  sched->n_items--;
}

static inline void
unschedule_node (EngineSchedule *sched,
		 EngineNode *node)
{
  guint leaf_level;
  
  g_return_if_fail (ENGINE_NODE_IS_SCHEDULED (node) == TRUE);
  leaf_level = node->sched_leaf_level;
  g_return_if_fail (leaf_level <= sched->leaf_levels);
  g_return_if_fail (sched->n_items > 0);
  
  /* SCHED_DEBUG ("unschedule_node(%p,%u)", node, leaf_level); */
  sched->nodes[leaf_level] = gsl_ring_remove (sched->nodes[leaf_level], node);
  node->sched_leaf_level = 0;
  node->sched_tag = FALSE;
  if (node->flow_jobs)
    _engine_mnl_reorder (node);
  sched->n_items--;
}

static inline void
unschedule_cycle (EngineSchedule *sched,
		  GslRing     *ring)
{
  guint leaf_level;
  GslRing *walk;
  
  g_return_if_fail (ENGINE_NODE_IS_SCHEDULED (ENGINE_NODE (ring->data)) == TRUE);
  leaf_level = ENGINE_NODE (ring->data)->sched_leaf_level;
  g_return_if_fail (leaf_level <= sched->leaf_levels);
  g_return_if_fail (sched->n_items > 0);
  
  /* SCHED_DEBUG ("unschedule_cycle(%p,%u,%p)", ring->data, leaf_level, ring); */
  sched->nodes[leaf_level] = gsl_ring_remove (sched->nodes[leaf_level], ring);
  for (walk = ring; walk; walk = gsl_ring_walk (walk, ring))
    {
      EngineNode *node = walk->data;
      
      if (!ENGINE_NODE_IS_SCHEDULED (node))
	g_warning ("node(%p) in schedule ring(%p) is untagged", node, ring);
      node->sched_leaf_level = 0;
      node->sched_tag = FALSE;
      if (node->flow_jobs)
	_engine_mnl_reorder (node);
    }
  sched->n_items--;
}

static void
_engine_schedule_debug_dump (EngineSchedule *sched)
{
  g_printerr ("sched(%p) = {\n", sched);
  if (sched)
    {
      guint i;
      
      g_printerr ("  n_items=%u, leaf_levels=%u, secured=%u,\n",
		  sched->n_items, sched->leaf_levels, sched->secured);
      g_printerr ("  in_pqueue=%u, cur_leaf_level=%u,\n",
		  sched->in_pqueue, sched->cur_leaf_level);
      g_printerr ("  cur_node=%p, cur_cycle=%p,\n",
		  sched->cur_node, sched->cur_cycle);
      for (i = 0; i < sched->leaf_levels; i++)
	{
	  GslRing *ring, *head = sched->nodes[i];
	  
	  if (!head)
	    continue;
	  g_printerr ("  { leaf_level=%u:", i);
	  for (ring = head; ring; ring = gsl_ring_walk (ring, head))
	    g_printerr (" node(%p(i:%u,s:%u))", ring->data,
			((EngineNode*) ring->data)->integrated,
			((EngineNode*) ring->data)->sched_tag);
	  g_printerr (" },\n");
	}
    }
  g_printerr ("};\n");
}


void
_engine_schedule_clear (EngineSchedule *sched)
{
  guint i;
  
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  g_return_if_fail (sched->in_pqueue == FALSE);
  
  while (sched->vnodes)
    unschedule_virtual (sched, sched->vnodes->data);
  for (i = 0; i < sched->leaf_levels; i++)
    {
      while (sched->nodes[i])
	unschedule_node (sched, sched->nodes[i]->data);
      while (sched->cycles[i])
	unschedule_cycle (sched, sched->cycles[i]->data);
    }
  g_return_if_fail (sched->n_items == 0);
}

void
_engine_schedule_destroy (EngineSchedule *sched)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  g_return_if_fail (sched->in_pqueue == FALSE);
  
  _engine_schedule_clear (sched);
  g_free (sched->nodes);
  g_free (sched->cycles);
  gsl_delete_struct (EngineSchedule, sched);
}

static void
_engine_schedule_grow (EngineSchedule *sched,
		       guint           leaf_level)
{
  guint ll = 1 << g_bit_storage (leaf_level);	/* power2 growth alignment, ll >= leaf_level+1 */
  
  if (sched->leaf_levels < ll)
    {
      guint i = sched->leaf_levels;
      
      sched->leaf_levels = ll;
      sched->nodes = g_renew (GslRing*, sched->nodes, sched->leaf_levels);
      sched->cycles = g_renew (GslRing*, sched->cycles, sched->leaf_levels);
      for (; i < sched->leaf_levels; i++)
	{
	  sched->nodes[i] = NULL;
	  sched->cycles[i] = NULL;
	}
    }
}

static void
schedule_virtual (EngineSchedule *sched,
		  EngineNode     *vnode)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  g_return_if_fail (vnode != NULL);
  g_return_if_fail (ENGINE_NODE_IS_VIRTUAL (vnode));
  g_return_if_fail (!ENGINE_NODE_IS_SCHEDULED (vnode));
  
  /* SCHED_DEBUG ("schedule_virtual(%p)", vnode); */
  vnode->sched_tag = TRUE;
  vnode->cleared_ostreams = FALSE;
  sched->vnodes = gsl_ring_append (sched->vnodes, vnode);
  sched->n_items++;
}

static void
schedule_node (EngineSchedule *sched,
	       EngineNode     *node,
	       guint           leaf_level)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  g_return_if_fail (node != NULL);
  g_return_if_fail (!ENGINE_NODE_IS_SCHEDULED (node));
  
  /* SCHED_DEBUG ("schedule_node(%p,%u)", node, leaf_level); */
  node->sched_leaf_level = leaf_level;
  node->sched_tag = TRUE;
  node->cleared_ostreams = FALSE;
  if (node->flow_jobs)
    _engine_mnl_reorder (node);
  _engine_schedule_grow (sched, leaf_level);
  /* could do 3-stage scheduling by expensiveness */
  sched->nodes[leaf_level] = (ENGINE_NODE_IS_EXPENSIVE (node) ? gsl_ring_prepend : gsl_ring_append) (sched->nodes[leaf_level], node);
  sched->n_items++;
}

static void
schedule_cycle (EngineSchedule *sched,
		GslRing        *cycle_nodes,
		guint           leaf_level)
{
  GslRing *walk;
  
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  g_return_if_fail (cycle_nodes != NULL);
  
  for (walk = cycle_nodes; walk; walk = gsl_ring_walk (walk, cycle_nodes))
    {
      EngineNode *node = walk->data;
      
      g_return_if_fail (!ENGINE_NODE_IS_SCHEDULED (node));
      node->sched_leaf_level = leaf_level;
      node->sched_tag = TRUE;
      node->cleared_ostreams = FALSE;
      if (node->flow_jobs)
	_engine_mnl_reorder (node);
    }
  _engine_schedule_grow (sched, leaf_level);
  sched->cycles[leaf_level] = gsl_ring_prepend (sched->cycles[leaf_level], cycle_nodes);
  sched->n_items++;
}

void
_engine_schedule_restart (EngineSchedule *sched)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == TRUE);
  g_return_if_fail (sched->cur_leaf_level == sched->leaf_levels);
  g_return_if_fail (sched->cur_node == NULL);
  g_return_if_fail (sched->cur_cycle == NULL);
  
  sched->cur_leaf_level = 0;
  if (sched->leaf_levels > 0)
    {
      sched->cur_node = sched->nodes[0];
      sched->cur_cycle = sched->cycles[0];
    }
}

void
_engine_schedule_secure (EngineSchedule *sched)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == FALSE);
  
  sched->secured = TRUE;
  sched->cur_leaf_level = sched->leaf_levels;
  
  if (gsl_debug_check (GSL_MSG_SCHED))
    _engine_schedule_debug_dump (sched);
}

static void
schedule_advance (EngineSchedule *sched)
{
  while (!sched->cur_node && !sched->cur_cycle && sched->cur_leaf_level < sched->leaf_levels)
    {
      sched->cur_leaf_level += 1;
      if (sched->cur_leaf_level < sched->leaf_levels)
	{
	  sched->cur_node = sched->nodes[sched->cur_leaf_level];
	  sched->cur_cycle = sched->cycles[sched->cur_leaf_level];
	}
    }
}

EngineNode*
_engine_schedule_pop_node (EngineSchedule *sched)
{
  g_return_val_if_fail (sched != NULL, NULL);
  g_return_val_if_fail (sched->secured == TRUE, NULL);
  g_return_val_if_fail (sched->cur_leaf_level <= sched->leaf_levels, NULL);
  
  do
    {
      guint leaf_level = sched->cur_leaf_level;
      
      if (sched->cur_node)
	{
	  EngineNode *node = sched->cur_node->data;
	  
	  sched->cur_node = gsl_ring_walk (sched->cur_node, sched->nodes[leaf_level]);
	  return node;
	}
      schedule_advance (sched);
    }
  while (sched->cur_node);
  
  /* nothing to hand out, either we're empty or still have cycles pending */
  return NULL;
}

GslRing*
_engine_schedule_pop_cycle (EngineSchedule *sched)
{
  g_return_val_if_fail (sched != NULL, NULL);
  g_return_val_if_fail (sched->secured == TRUE, NULL);
  g_return_val_if_fail (sched->cur_leaf_level <= sched->leaf_levels, NULL);
  
  do
    {
      guint leaf_level = sched->cur_leaf_level;
      
      if (sched->cur_cycle)
	{
	  GslRing *cycle = sched->cur_cycle->data;
	  
	  sched->cur_cycle = gsl_ring_walk (sched->cur_cycle, sched->cycles[leaf_level]);
	  return cycle;
	}
      schedule_advance (sched);
    }
  while (sched->cur_cycle);
  
  /* nothing to hand out, either we're empty or still have nodes pending */
  return NULL;
}

void
_engine_schedule_unsecure (EngineSchedule *sched)
{
  g_return_if_fail (sched != NULL);
  g_return_if_fail (sched->secured == TRUE);
  g_return_if_fail (sched->in_pqueue == FALSE);
  g_return_if_fail (sched->cur_leaf_level == sched->leaf_levels);
  g_return_if_fail (sched->cur_node == NULL);
  g_return_if_fail (sched->cur_cycle == NULL);
  
  sched->secured = FALSE;
  sched->cur_leaf_level = ~0;
}

void
_engine_schedule_consumer_node (EngineSchedule *schedule,
				EngineNode     *node)
{
  EngineQuery query = { 0, };
  
  g_return_if_fail (schedule != NULL);
  g_return_if_fail (schedule->secured == FALSE);
  g_return_if_fail (node != NULL);
  g_return_if_fail (ENGINE_NODE_IS_CONSUMER (node));
  g_return_if_fail (ENGINE_NODE_IS_VIRTUAL (node) == FALSE);
  
  subschedule_query_node (schedule, node, &query);
  g_assert (query.cycles == NULL);	/* paranoid */
  g_assert (query.cycle_nodes == NULL);	/* paranoid */
  schedule_node (schedule, node, query.leaf_level);
}


/* --- depth scheduling --- */
static void
update_suspension_state (EngineNode *node)
{
  GslRing *ring;
  guint seen_suspended = 0, seen_active = 0;
  gboolean outputs_suspended;

  node->suspension_update = FALSE;
  for (ring = node->output_nodes; ring && !seen_active; ring = gsl_ring_walk (ring, node->output_nodes))
    {
      EngineNode *dest_node = ring->data;
      if (dest_node != node)	/* self-feedback cycles */
	{
	  if (dest_node->suspension_update)
	    update_suspension_state (dest_node);
	  if (ENGINE_NODE_IS_SUSPENDED (dest_node))
	    seen_suspended++;
	  else
	    seen_active++;
	}
    }
  outputs_suspended = seen_suspended && !seen_active;
  if (outputs_suspended != node->outputs_suspended)
    {
      node->outputs_suspended = outputs_suspended;
      node->needs_reset = node->module.klass->reset != NULL;
    }
  /* FIXME: suspension state updates need back propagation within cycles */
}

static GslRing*
merge_untagged_node_lists_uniq (GslRing *ring1,
				GslRing *ring2)
{
  GslRing *walk;
  
  /* paranoid, ensure all nodes are untagged (ring2) */
  for (walk = ring2; walk; walk = gsl_ring_walk (walk, ring2))
    {
      EngineNode *node = walk->data;
      
      g_assert (node->sched_recurse_tag == FALSE);
    }
  
  /* tag all nodes in ring1 first */
  for (walk = ring1; walk; walk = gsl_ring_walk (walk, ring1))
    {
      EngineNode *node = walk->data;
      
      g_assert (node->sched_recurse_tag == FALSE);	/* paranoid check */
      node->sched_recurse_tag = TRUE;
    }
  
  /* merge list with missing (untagged) nodes */
  for (walk = ring2; walk; walk = gsl_ring_walk (walk, ring2))
    {
      EngineNode *node = walk->data;
      
      if (node->sched_recurse_tag == FALSE)
	ring1 = gsl_ring_append (ring1, node);
    }
  
  /* untag all nodes */
  for (walk = ring1; walk; walk = gsl_ring_walk (walk, ring1))
    {
      EngineNode *node = walk->data;
      
      node->sched_recurse_tag = FALSE;
    }
  for (walk = ring2; walk; walk = gsl_ring_walk (walk, ring2))
    {
      EngineNode *node = walk->data;
      
      node->sched_recurse_tag = FALSE;
    }
  gsl_ring_free (ring2);
  return ring1;
}

static gboolean
resolve_cycle (EngineCycle *cycle,
	       EngineNode  *node,
	       GslRing    **cycle_nodes_p)
{
  if (node != cycle->last)
    return FALSE;
  if (!cycle->seen_deferred_node)
    {
      g_error ("cycle without delay module: (%p)", cycle);
    }
  *cycle_nodes_p = merge_untagged_node_lists_uniq (*cycle_nodes_p, cycle->nodes);
  cycle->nodes = NULL;
  cycle->last = NULL;
  return TRUE;
}

static gboolean
master_resolve_cycles (EngineQuery *query,
		       EngineNode  *node)
{
  GslRing *walk;
  gboolean all_resolved = TRUE;
  
  g_assert (query->cycles != NULL);	/* paranoid */
  
  walk = query->cycles;
  while (walk)
    {
      GslRing *next = gsl_ring_walk (walk, query->cycles);
      EngineCycle *cycle = walk->data;
      
      if (resolve_cycle (cycle, node, &query->cycle_nodes))
	{
	  g_assert (cycle->last == NULL);	/* paranoid */
	  g_assert (cycle->nodes == NULL);	/* paranoid */
	  
	  gsl_delete_struct (EngineCycle, cycle);
	  query->cycles = gsl_ring_remove_node (query->cycles, walk);
	}
      else
	all_resolved = FALSE;
      walk = next;
    }
  if (all_resolved)
    g_assert (query->cycles == NULL);	/* paranoid */
  return all_resolved;
}

static void
query_add_cycle (EngineQuery *query,
		 EngineNode  *dep,
		 EngineNode  *node)
{
  EngineCycle *cycle = gsl_new_struct0 (EngineCycle, 1);
  
  cycle->last = dep;
  cycle->nodes = gsl_ring_prepend (NULL, node);
  cycle->seen_deferred_node = ENGINE_NODE_IS_DEFERRED (node); /* dep will be checked when added to nodes */
  query->cycles = gsl_ring_append (query->cycles, cycle);
}

static void
query_merge_cycles (EngineQuery *query,
		    EngineQuery *child_query,
		    EngineNode  *node)
{
  GslRing *walk;
  
  g_assert (child_query->cycles != NULL);	/* paranoid */
  
  /* add node to all child cycles */
  for (walk = child_query->cycles; walk; walk = gsl_ring_walk (walk, child_query->cycles))
    {
      EngineCycle *cycle = walk->data;
      
      cycle->nodes = gsl_ring_prepend (cycle->nodes, node);
      cycle->seen_deferred_node |= ENGINE_NODE_IS_DEFERRED (node);
    }
  
  /* merge child cycles into our cycle list */
  query->cycles = gsl_ring_concat (query->cycles, child_query->cycles);
  child_query->cycles = NULL;
  
  /* merge child's cycle nodes from resolved cycles into ours */
  query->cycle_nodes = merge_untagged_node_lists_uniq (query->cycle_nodes, child_query->cycle_nodes);
  child_query->cycle_nodes = NULL;
}

static inline void
clean_ostreams (EngineNode *node)
{
  if (!node->cleared_ostreams && !ENGINE_NODE_IS_SCHEDULED (node))
    {
      guint i;

      for (i = 0; i < ENGINE_NODE_N_OSTREAMS (node); i++)
	node->module.ostreams[i].connected = FALSE;
      node->cleared_ostreams = TRUE;
    }
}

static inline EngineNode*
subschedule_skip_virtuals (EngineSchedule *schedule,
			   EngineNode     *node,
			   guint          *ostream_p)
{
  while (node && ENGINE_NODE_IS_VIRTUAL (node))
    {
      EngineInput *input = node->inputs + *ostream_p;
      if (!ENGINE_NODE_IS_SCHEDULED (node))
	schedule_virtual (schedule, node);
      node = input->src_node;
      *ostream_p = input->src_stream;
    }
  return node;
}

static inline void
subschedule_child (EngineSchedule *schedule,
		   EngineNode     *node,
		   EngineQuery    *query,
		   EngineNode     *child,
		   guint           child_ostream)
{
  g_return_if_fail (ENGINE_NODE_IS_VIRTUAL (node) == FALSE);

  /* flag connected ostream */
  clean_ostreams (child);
  child->module.ostreams[child_ostream].connected = TRUE;

  /* schedule away if necessary */
  if (ENGINE_NODE_IS_SCHEDULED (child))
    query->leaf_level = MAX (query->leaf_level, child->sched_leaf_level + 1);
  else if (child->sched_recurse_tag)	/* cycle */
    query_add_cycle (query, child, node);
  else			/* nice boy ;) */
    {
      EngineQuery child_query = { 0, };
      
      subschedule_query_node (schedule, child, &child_query);
      query->leaf_level = MAX (query->leaf_level, child_query.leaf_level + 1);
      if (!child_query.cycles)
	{
	  g_assert (child_query.cycle_nodes == NULL);	/* paranoid */
	  schedule_node (schedule, child, child_query.leaf_level);
	}
      else if (master_resolve_cycles (&child_query, child))
	{
	  g_assert (child == child_query.cycle_nodes->data);	/* paranoid */
	  schedule_cycle (schedule, child_query.cycle_nodes, child_query.leaf_level);
	  child_query.cycle_nodes = NULL;
	}
      else
	query_merge_cycles (query, &child_query, node);
      g_assert (child_query.cycles == NULL && child_query.cycle_nodes == NULL);	/* paranoid */
    }
}

static void
subschedule_query_node (EngineSchedule *schedule,
			EngineNode     *node,
			EngineQuery    *query)
{
  guint i, j;
  
  g_return_if_fail (ENGINE_NODE_IS_VIRTUAL (node) == FALSE);
  g_return_if_fail (node->sched_recurse_tag == FALSE);
  g_return_if_fail (query->leaf_level == 0);

  /* update suspension state if necessary */
  if (node->suspension_update)
    update_suspension_state (node);
  /* reset ostream[].connected flags */
  clean_ostreams (node);
  
  /* SCHED_DEBUG ("sched_query(%p)", node); */
  node->sched_recurse_tag = TRUE;
  /* schedule input stream children */
  for (i = 0; i < ENGINE_NODE_N_ISTREAMS (node); i++)
    {
      EngineNode *child = node->inputs[i].src_node;
      guint child_ostream = node->inputs[i].src_stream;

      child = subschedule_skip_virtuals (schedule, child, &child_ostream);
      if (!child)
	{
	  node->module.istreams[i].connected = FALSE;
	  node->inputs[i].real_node = NULL;
	}
      else
	{
	  node->module.istreams[i].connected = TRUE;
	  node->inputs[i].real_node = child;
	  node->inputs[i].real_stream = child_ostream;
	  subschedule_child (schedule, node, query, child, child_ostream);
	}
    }
  /* eliminate dead virtual ends in jstreams */
  for (j = 0; j < ENGINE_NODE_N_JSTREAMS (node); j++)
    {
      GslJStream *jstream = node->module.jstreams + j;

      /* we check this jstream's connections for virtual dead-ends.
       * valid connections stay at (are moved to) the array front and
       * are counted in n_connections, while dead-ends are moved
       * to the array end and are counted in i. jcount is the number
       * of total connections for this joint stream.
       */
      jstream->n_connections = i = 0;
      while (jstream->n_connections + i < jstream->jcount)
	{
	  EngineJInput *tmp, *jinput = node->jinputs[j] + jstream->n_connections;
	  EngineNode *child = jinput->src_node;
	  guint child_ostream = jinput->src_stream;

	  child = subschedule_skip_virtuals (schedule, child, &child_ostream);
	  if (child)
	    {
	      jstream->n_connections++;		/* valid input */
	      jinput->real_node = child;
	      jinput->real_stream = child_ostream;
	    }
	  else
	    {
	      i++;				/* virtual dead-end */
	      /* swap dead-end out of the way */
	      tmp = node->jinputs[j] + jstream->jcount - i;
	      if (jinput != tmp)
		{
		  child = tmp->src_node;
		  child_ostream = tmp->src_stream;
		  tmp->src_node = jinput->src_node;
		  tmp->src_stream = jinput->src_stream;
		  jinput->src_node = child;
		  jinput->src_stream = child_ostream;
		}
	    }
	}
    }
  /* schedule valid jstream connections */
  for (j = 0; j < ENGINE_NODE_N_JSTREAMS (node); j++)
    for (i = 0; i < node->module.jstreams[j].n_connections; i++)
      {
	EngineNode *child = node->jinputs[j][i].real_node;
	guint child_ostream = node->jinputs[j][i].real_stream;
	
	subschedule_child (schedule, node, query, child, child_ostream);
      }
  node->counter = GSL_TICK_STAMP;
  node->sched_recurse_tag = FALSE;
  /* SCHED_DEBUG ("sched_done(%p)", node); */
}
