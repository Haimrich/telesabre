from dataclasses import dataclass

@dataclass
class Config:
    """Configuration class for the experiment."""
    name: str = "default"
    energy_type: str = "extended_set"
    decay_factor: float = 0.9
    decay_reset: int = 5
    optimize_initial: bool = False
    teleport_bonus: int = 100 # 100
    telegate_bonus: int = 100 #-1000 # 100 # 1000 #-1000 # disable telegates
    safety_valve_iters : int = 100
    extended_set_size : int = 20
    full_core_penalty : int = 10
    save_data : bool = False
    max_solving_deadlock_iterations: int = 300
    
    swap_decay : float = 0.002
    teleport_decay : float = 0.005
    telegate_decay : float = 0.005
    
    initial_layout_hun_like : bool = True
    
    def __repr__(self):
        return f"Name: {self.name}\n" \
                f"Energy Type: {self.energy_type}\n" \
                f"Decay Factor: {self.decay_factor}\n" \
                f"Decay Reset: {self.decay_reset}\n"
                