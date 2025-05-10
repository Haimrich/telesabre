import numpy as np
from numba import njit, jit, prange
from tqdm import trange
from scipy.optimize import linear_sum_assignment

# === FGP-rOEE ===

def random_partition(graph, k):
    """
    Generates a random partition assignment for each node.
    
    Args:
        graph: A NumPy array representing the weighted adjacency matrix of the graph.
        k: The number of partitions.
    
    Returns:
        A list representing the partition assignment for each node.
    """
    num_nodes = graph.shape[0]
    return np.random.permutation(num_nodes) % k
    #return np.arange(num_nodes) % k

@njit(cache=True)
def partition_cost(graph, partition):
    """
    Calculates the cost of a partition.
    
    Args:
        graph: A NumPy array representing the weighted adjacency matrix of the graph.
        partition: A list representing the partition assignment for each node.
    
    Returns:
        The cost of the partition.
    """
    cost = 0
    for i in range(graph.shape[0]):
        for j in range(i):
            if partition[i] != partition[j]:
                cost += graph[i, j]
    return cost


def partition_cost_numpy(graph, partition):
  n = graph.shape[0]
  indexes = np.tile(np.arange(n).reshape(-1,1), n)
  mask = (indexes < indexes.T) & (partition[indexes] != partition[indexes.T])
  return np.sum(graph[mask])



def oee_algorithm(graph, k, initial, relaxed=False):
  """
  Performs OEE algorithm for k-way graph partitioning

  Args:
    graph: A NumPy array representing the weighted adjacency matrix of the graph.
    k: The number of partitions.

  Returns:
    A list representing the final partition assignment for each node.
  """
  
  assert np.allclose(graph, graph.T), "Error: Graph is not symmetric"
  # Step 0: Initial random partition
  partition = initial.copy()
  
  # Print initial cost
  print("Initial Cost:", partition_cost(graph, partition)) #, "- Partition:", partition)

  
  n = graph.shape[0]
  
  old_cost = float('inf')
  # Loop for overall extreme exchange
  while True:
    # Step 1: Initialization
    C = np.arange(n)
    
    hyphyp_partition = partition.copy()
    
    steps = []
    while True:
      gain_max = -float('inf')
      gain_index = None
      # Step 2: Find maximum gain pair
      for i in C:
        for j in C[C < i]:
          if partition[i] != partition[j]:
            hyp_partition = hyphyp_partition.copy()
            hyp_partition[i], hyp_partition[j] = hyp_partition[j], hyp_partition[i]
            hyp_gain = partition_cost(graph, hyphyp_partition) - partition_cost(graph, hyp_partition)
            
            gain = hyp_gain
            if gain >= gain_max:
              gain_max = gain
              gain_index = (i, j)
            
      #print("Number of combinations:", num_comb)

      if gain_index is None:
        break 
      
      C = C[(C != gain_index[0]) & (C != gain_index[1])].copy()
      steps.append((gain_max, gain_index))
      
      hyphyp_partition[gain_index[0]], hyphyp_partition[gain_index[1]] = hyphyp_partition[gain_index[1]], hyphyp_partition[gain_index[0]]
        
      # Step 4
      if len(C) == 0:
        break
      
    # Step 5: find m
    #assert len(steps) == n // 2, f"Error: Number of steps is not equal to {n//2} but {len(steps)}"
    
    m = max(range(0,len(steps)+1), key=lambda x: sum(s[0] for s in steps[:x]))
    for m_ in range(m):
      a, b = steps[m_][1]
      partition[a], partition[b] = partition[b], partition[a]
    
    # Print cost after iteration
    g_max = sum(s[0] for s in steps[:m])
    
    cost = partition_cost(graph, partition)
    print("Cost:", cost, "- m:", m, "- Gain:", g_max) #, " - Partition:", partition)
    
    if g_max == 0:
      print("g_max is 0")
      break
    
    if relaxed and cost < 1e10:
      print("Relaxed Stop.")
      break
    
    if cost >= old_cost:
      print("Cost increased.")
      break
      
    old_cost = cost

  return partition



def oee_algorithm_old(graph, k):
  """
  Performs OEE algorithm for k-way graph partitioning

  Args:
    graph: A NumPy array representing the weighted adjacency matrix of the graph.
    k: The number of partitions.

  Returns:
    A list representing the final partition assignment for each node.
  """
  
  assert np.allclose(graph, graph.T), "Error: Graph is not symmetric"
  # Step 0: Initial random partition
  partition = random_partition(graph, k)
  
  # Print initial cost
  print("Initial Cost:", partition_cost(graph, partition), "- Partition:", partition)

  
  n = graph.shape[0]
  

  # Loop for overall extreme exchange
  while True:
    # Step 1: Initialization
    C = np.arange(n)
    
    W = np.zeros((n, k), dtype=np.float64)
    for i in range(n):
      for j in range(n):
        W[i, partition[j]] += graph[i, j]
        
    D = np.zeros((n, k), dtype=np.float64)
    for i in range(n):
      D[i,:] = W[i,:] - W[i, partition[i]]

    
    hyphyp_partition = partition.copy()
    
    steps = []
    num_comb = 0
    while True:
      gain_max = -float('inf')
      gain_index = None
      # Step 2: Find maximum gain pair
      for i in C:
        for j in C:
          if j < i:
            gain = 0
            if partition[i] != partition[j]:
              gain = D[i, partition[j]] + D[j, partition[i]] - 2 * graph[i, j]
              hyp_partition = hyphyp_partition.copy()
              hyp_partition[i], hyp_partition[j] = hyp_partition[j], hyp_partition[i]
              hyp_gain = partition_cost(graph, hyphyp_partition) - partition_cost(graph, hyp_partition)
              
              #assert np.round(gain, 4) == np.round(hyp_gain, 4), f"Error: gain is not equal to hyp_gain ({hyp_gain}) but {gain}"
              gain = hyp_gain
            if gain >= gain_max:
              gain_max = gain
              gain_index = (i, j)
            num_comb += 1
            
      #print("Number of combinations:", num_comb)

      if gain_index is None:
        #assert False, "Error: gain_index is None"
        break 
      
      C = C[(C != gain_index[0]) & (C != gain_index[1])].copy()
      #print("Removed:", gain_index, "- P:", partition[gain_index[0]], partition[gain_index[1]])
      #print("CO:", C)
      #print("CC", partition[C])
      steps.append((gain_max, gain_index))
      hyphyp_partition[gain_index[0]], hyphyp_partition[gain_index[1]] = hyphyp_partition[gain_index[1]], hyphyp_partition[gain_index[0]]
      
      # Step 3: Update D
      a, b = gain_index
      for i in C:
        
        if partition[i] != partition[a] and partition[i] != partition[b]:
          D[i, partition[a]] = D[i, partition[a]] + graph[i, b] - graph[i, a]
        elif partition[i] == partition[b]:
          D[i, partition[a]] = D[i, partition[a]] + 2*graph[i, b] - 2*graph[i, a]
          
        if partition[i] != partition[a] and partition[i] != partition[b]:
          D[i, partition[b]] = D[i, partition[b]] + graph[i, a] - graph[i, b]
        elif partition[i] == partition[a]:
          D[i, partition[b]] = D[i, partition[b]] + 2*graph[i, a] - 2*graph[i, b]
          
        for l in range(k):
          if partition[i] == partition[a] and l != partition[a] and l != partition[b]:
            D[i, l] = D[i, l] + graph[i, a] - graph[i, b]
          elif partition[i] == partition[b] and l != partition[a] and l != partition[b]:
            D[i, l] = D[i, l] + graph[i, b] - graph[i, a]
          else:
            pass
        
      # Step 4
      if len(C) == 0:
        break
      
    # Step 5: find m
    #assert len(steps) == n // 2, f"Error: Number of steps is not equal to {n//2} but {len(steps)}"
    
    m = max(range(1,len(steps)+1), key=lambda x: sum(s[0] for s in steps[:x]))
    for m_ in range(m):
      a, b = steps[m_][1]
      pre_cost = partition_cost(graph, partition)
      partition[a], partition[b] = partition[b], partition[a]
      post_cost = partition_cost(graph, partition)
      print("   Cost:", post_cost, "- m:", m, "- Gain:", steps[m_][0])
      
      assert np.round(pre_cost - post_cost, 4) == np.round(steps[m_][0],4), f"Error: pre_cost - post_cost is not equal to steps[m_][0] ({steps[m_][0]}) but {pre_cost - post_cost}"
    
    g_max = sum(s[0] for s in steps[:m])
    if g_max == 0:
      print("g_max is 0")
      break
      
    # Print cost after iteration
    print("Cost:", partition_cost(graph, partition), "- m:", m, "- Gain:", g_max, " - Partition:", partition)

  return partition

# Fine Grained Partitioning
def lookahead_weight_func(n, sigma = 1):
    # exponential decay
    return 2.0**(-n/sigma)
    # constant function
    return 1 if n <= sigma else 0
    # gaussian decay
    return np.exp(-n**2/(2*sigma**2))
  

@njit(cache=True)
def fgp_oee_weight(t, slices, num_qubits):
    num_slices = len(slices)
    weights = np.zeros((num_qubits, num_qubits), dtype=np.float64)
    for q1 in range(num_qubits):
        for q2 in range(q1):
            # lookahead weights
            for m in range(t+1, num_slices):
                if ((slices[m][:,0] == q1) & (slices[m][:,0] == q2)).any() or ((slices[m][:,1] == q1) & (slices[m][:,0] == q2)).any():
                    weights[q1, q2] += 2.0**(t-m)
                    weights[q2, q1] += 2.0**(t-m)
            if ((slices[t][:,0] == q1) & (slices[t][:,0] == q2)).any() or ((slices[t][:,1] == q1) & (slices[t][:,0] == q2)).any():
                weights[q1, q2] = 1e10 # inf
                weights[q2, q1] = 1e10 # inf
    return weights
  

def fgp_oee_assignment(slices, num_qubits, num_cores, capacity, relaxed = False):

  assignments = []
  
  num_slices = len(slices)
  for t in trange(0, num_slices):
      if False:
        weights = np.zeros((num_qubits, num_qubits), dtype=np.float64)
      
        for q1 in range(num_qubits):
            for q2 in range(num_qubits):
                # lookahead weights
                for m in range(t+1, num_slices):
                    if (q1, q2) in slices[m] or (q2, q1) in slices[m]:
                        weights[q1, q2] += lookahead_weight_func(m-t)
                if (q1, q2) in slices[t] or (q2, q1) in slices[t]:
                    weights[q1, q2] = 1e10 # inf
      else:
        weights = fgp_oee_weight(t, [np.array(s) for s in slices], num_qubits)
        
        
      initial = assignments[t-1] if t > 0 else random_partition(weights, num_cores)
      cores = oee_algorithm(weights, num_cores, initial=initial, relaxed = relaxed)
      assignments.append(cores)

  return assignments



# === Hungarian Assignament ===


def initial_assignement(slices, num_qubits, num_cores, capacity) -> tuple:
  assignement = np.full(num_qubits, -1)
  capacities = np.full(num_cores, capacity)
  for q1, q2 in slices[0]:
    for c in range(num_cores):
      if capacities[c] >= 2:
        assignement[q1] = c
        assignement[q2] = c
        capacities[c] -= 2
        break
  for q in range(num_qubits):
    if assignement[q] == -1:
      for c in range(num_cores):
        if capacities[c] >= 1:
          assignement[q] = c
          capacities[c] -= 1
          break
  return assignement, capacities


def hungarian(cost_matrix):
  gate_ids, core_ids = linear_sum_assignment(cost_matrix)
  cost = cost_matrix[gate_ids, core_ids].sum()
  return gate_ids, core_ids, cost


@njit(cache=True)
def calculate_attraction(slices, unfeasible_gates, current, t, num_qubits, num_cores):
    num_gates = len(unfeasible_gates)
    # Equation 20
    attr_q_t = np.zeros((num_qubits, num_cores), dtype=np.float64)
    for q in range(num_qubits):
      for c in range(num_cores):
        for q_ in range(num_qubits):
          if current[q_] == c:
            for m in range(t+1, len(slices)):
              if ((slices[m][:,0] == q) & (slices[m][:,1] == q_)).any() or ((slices[m][:,0] == q_) & (slices[m][:,1] == q)).any():
              #if ((slices[m][0] == q) & (slices[m][1] == q_)).any():
              #if (q, q_) in slices[m] or (q_, q) in slices[m]:
                attr_q_t[q, c] += 2.0 ** (t - m)

    
    # Equation 21
    attr_g_t = np.empty((num_gates, num_cores), dtype=np.float64)
    for g, (q1, q2) in enumerate(unfeasible_gates):
      attr_g_t[g] = (attr_q_t[q1] + attr_q_t[q2]) / 2
    
    return attr_g_t


def hungarian_assignement(slices, num_qubits, num_cores, capacity, lookahead=False, distance_matrix=None, initial=None):
  
  # Initial Assignment
  current, cur_capacities = initial_assignement(slices, num_qubits, num_cores, capacity)
  if initial is not None:
    current, cur_capacities = initial.copy(), capacity - np.bincount(initial, minlength=num_cores)
  assignments = [current.copy()]
  
  #print('IA', current)
  #print('IC', cur_capacities)
  unfeasible_gates = []
  slice_arr = [np.array(s) for s in slices]
  
  for t in range(1, len(slices)):
    for q1, q2 in slices[t]:
      if current[q1] != current[q2]:
        unfeasible_gates.append((q1, q2))
        # remove unfeasible gates from assignment
        cur_capacities[current[q1]] += 1
        cur_capacities[current[q2]] += 1  
    
    #print('unfeasible_gates', len(unfeasible_gates))
    #print('avaialble spots', sum(cur_capacities))
    
    if True:
      """
      From: https://arxiv.org/pdf/2309.12182.pdf
        Each core must contain an even number of qubits interacting
        in unfeasible two-qubit gates for this approach to work.
        Otherwise, when assigning operations into cores, there will be
        a pair of qubits left to assign and two cores with exactly one
        free space each, making it impossible to assign an operation
        to the core. An auxiliary two-qubit gate involving two 
        noninteracting qubits from the cores with an odd number of
        qubits involved in unfeasible operations is created to solve
        this, ensuring that all cores contain an even number of free
        spaces and that all two-qubit operations will be allocated.
      """
      aux_gate = []
      used_qubits = np.array(slices[t]).flatten().tolist()
      for c in range(num_cores):
        if cur_capacities[c] % 2 == 1:
          for q in range(num_qubits):
            if current[q] == c and q not in used_qubits:
                cur_capacities[c] += 1
                aux_gate.append(q)
                break
      assert len(aux_gate) % 2 == 0, f"Error: Number of auxiliary gates is not even but is {len(aux_gate)}"
      if len(aux_gate) > 0:
        unfeasible_gates.extend([(aux_gate[i], aux_gate[i+1]) for i in range(0, len(aux_gate), 2)])
    
    #print('unfeasible_gates + aux', len(unfeasible_gates))
    #print('avaialble spots', sum(cur_capacities))
    #print(cur_capacities)
    
    while (num_gates := len(unfeasible_gates)) > 0:
      if lookahead:
          attr_g_t  = calculate_attraction(slice_arr, np.array(unfeasible_gates), current, t, num_qubits, num_cores)
          if False:
            # Equation 20
            attr_q_t = np.zeros((num_qubits, num_cores), dtype=np.float64)
            for q in range(num_qubits):
              for c in range(num_cores):
                for q_ in range(num_qubits):
                  if current[q_] == c:
                    for m in range(t+1, len(slices)):
                      if (q, q_) in slices[m] or (q_, q) in slices[m]:
                        attr_q_t[q, c] += 2 ** (t - m)
            # Equation 21
            attr_g_t = np.empty((num_gates, num_cores), dtype=np.float64)
            for g, (q1, q2) in enumerate(unfeasible_gates):
              attr_g_t[g] = (attr_q_t[q1] + attr_q_t[q2]) / 2
      else:
          attr_g_t = np.zeros((num_gates, num_cores), dtype=np.float64)
      
      # Equation 22
      cost_matrix = np.zeros((num_gates, num_cores), dtype=np.float64)
      if distance_matrix is None:
        for g, (q1, q2) in enumerate(unfeasible_gates):
          for c in range(num_cores):
            if cur_capacities[c] == 0:
              cost_matrix[g, c] = 1e4
            elif current[q1] == c or current[q2] == c:
              cost_matrix[g, c] = 1 - attr_g_t[g, c]
            else:
              cost_matrix[g, c] = 2 - attr_g_t[g, c]
      else:
        for g, (q1, q2) in enumerate(unfeasible_gates):
          for c in range(num_cores):
            if cur_capacities[c] == 0:
              cost_matrix[g, c] = 1e4
            else:
              c1, c2 = current[[q1,q2]]
              cost_matrix[g, c] = distance_matrix[c1, c] + distance_matrix[c2, c] - attr_g_t[g, c]
      
      #print('cur_cap:', cur_capacities)
      #print('remaining_gates:', len(unfeasible_gates), 'available spots:', sum(cur_capacities), 'needed_spots:', len(unfeasible_gates)*2)
      assert sum(cur_capacities % 2) == 0
      
      assigned_gates, assigned_cores, cost = hungarian(cost_matrix)
      #print(assigned_gates, assigned_cores)
      #print('cost:', cost, 'shape', cost_matrix.shape)
      unfeasible_gates_new = unfeasible_gates.copy()
      for g, c in zip(assigned_gates, assigned_cores):
        if cost_matrix[g, c] < 1e3:
          q1, q2 = unfeasible_gates[g]
          current[q1] = c
          current[q2] = c
          cur_capacities[c] -= 2
          unfeasible_gates_new.remove((q1, q2))
        
      unfeasible_gates = unfeasible_gates_new
    
    assert sum(cur_capacities) == 0, f"Error: Sum of cur_capacities is not 0 but {sum(cur_capacities)}"
    assignments.append(current.copy())
  
  return assignments
  
  
# === Utils ===


def count_non_valid_assignments(slices, assignments, capacity=10):
  non_valid = 0
  exceed_cap = 0
  sep_friends = 0
  for t, gates in enumerate(slices):
      cores = assignments[t]
      _, capacities = np.unique(cores, return_counts=True)
      if np.any(capacities > capacity):
        non_valid += 1
        exceed_cap += 1
      else:
        non_valid_flag = False
        for q1, q2 in gates:
            if cores[q1] != cores[q2]:
              non_valid_flag = True
              sep_friends += 1
        if non_valid_flag:
          non_valid += 1  

  #print('Non-valid timesteps:', non_valid)
  return non_valid, exceed_cap, sep_friends


def count_intercore_comms(assignments, distance_matrix=None, tp_time=5, ports=1):
    num_slices, num_qubits = len(assignments), len(assignments[0])

    depth = 0
    intercore_comms = 0
    for t in range(num_slices - 1):
        connections = []
        cores_now, cores_after = assignments[t], assignments[t + 1]
        for q in range(num_qubits):
            if cores_now[q] != cores_after[q]:
                distance = (
                    1 if distance_matrix is None
                    else distance_matrix[cores_now[q], cores_after[q]].item()
                )
                intercore_comms += distance
                connections.append((min(cores_now[q], cores_after[q]), max(cores_now[q], cores_after[q])))
                
        # Count unique connections
        connections, conn_counts = np.unique(connections, axis=0, return_counts=True)        
        depth += max([distance_matrix[conn[0], conn[1]] * np.ceil(conn_counts[c] / ports) * tp_time  for c, conn in enumerate(connections)] + [0])

    return intercore_comms, depth

# === Main ===


if __name__ == "__main__":
  # Example usage
  graph = [[0, 20,  0,  0,  1,  2,  0,  0],
          [20,  0,  1,  0,  1,  2,  0,  5],
          [ 0,  1,  0, 10,  0,  5,  1,  1],
          [ 0,  0, 10,  0,  0,  0,  2,  2],
          [ 1,  1,  0,  0,  0, 20,  0,  0],
          [ 2,  2,  5,  0, 20,  0,  0,  0],
          [ 0,  0,  1,  2,  0,  0,  0, 10],
          [ 0,  5,  1,  2,  0,  0, 10,  0]]

  # Convert the nested list into a NumPy array
  graph = np.array(graph)
  k = 2  # Number of partitions

  partition = oee_algorithm(graph, k)
  print("Final Partition:", partition)
