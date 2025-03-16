import json
 
import numpy as np
 
 
    
class SparseBucketPriorityQueue:
    def __init__(self):
        self.buckets = {}  # Maps priority → set of items with that priority
        self.item_to_priority = {}  # Maps item → its priority
        self.min_priority = float('inf')
        
    def add_or_update(self, item, priority):
        # Remove from old bucket if exists
        if item in self.item_to_priority:
            old_priority = self.item_to_priority[item]
            self.buckets[old_priority].remove(item)
            
            # Clean up empty buckets
            if not self.buckets[old_priority]:
                del self.buckets[old_priority]
                # Update min if needed
                if old_priority == self.min_priority:
                    self.min_priority = float('inf')
                    if self.buckets:
                        self.min_priority = min(self.buckets.keys())
        
        # Add to new bucket
        if priority not in self.buckets:
            self.buckets[priority] = set()
        self.buckets[priority].add(item)
        self.item_to_priority[item] = priority
        
        # Update min_priority
        self.min_priority = min(self.min_priority, priority)
        
    def remove_item(self, item):
        if item in self.item_to_priority:
            priority = self.item_to_priority[item]
            self.buckets[priority].remove(item)
            del self.item_to_priority[item]
            
            # Clean up empty bucket
            if not self.buckets[priority]:
                del self.buckets[priority]
                
                # Update min_priority if we removed the min item
                if priority == self.min_priority:
                    self.min_priority = float('inf')
                    if self.buckets:
                        self.min_priority = min(self.buckets.keys())
    
    def get_min(self):
        if self.min_priority == float('inf'):
            return None
        return next(iter(self.buckets[self.min_priority]))
    
    def get_min_priority(self):
        return self.min_priority
    
    def is_empty(self):
        return len(self.item_to_priority) == 0
    
    
    

class NpEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return super(NpEncoder, self).default(obj)