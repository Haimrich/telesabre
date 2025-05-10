function enablePanZoom() {
    let dag_scene = document.querySelector('#dag-scene')
    panzoom(dag_scene)

    let layout_scene = document.querySelector('#layout-scene')
    panzoom(layout_scene)
}


let data;
let nodes = [];
let nodes_text = [];
let edges = [];
let edge_to_id = {};
let nodes_coordiantes = [];
let colors = [];

let gates = [];
let dependencies = [];
let gate_to_edge_ids = [];

let needed_edges = [];

const POS_SCALE = 150;
const POS_SCALE_CIR = 600;
const Y_SCALE_CIR = 1.5;
const X_SCALE_CIR = 1.5;

function renderIteration(iteration) {
    console.log(data.iterations[iteration]);

    document.getElementById('iteration').textContent = iteration;
    document.getElementById('energy').textContent = data.iterations[iteration].energy[0].toFixed(3);

    const num_virt_qubits = data.circuit.num_qubits;
    for (let i = 0; i < nodes.length; i++) {
        if (i < num_virt_qubits) {
            nodes[i].style.fill = colors[i];
            nodes_text[i].style.opacity = "1";
        } else {
            nodes[i].style.fill = "white";
            nodes_text[i].style.opacity = "0";
        }

        p = data.iterations[iteration].virt_to_phys[i];
        const [x, y] = nodes_coordiantes[p];
        nodes[i].setAttribute("cx", x);
        nodes[i].setAttribute("cy", y);

        //nodes_text[i].setAttribute("x", x);
        //nodes_text[i].setAttribute("y", y);

        const [ox, oy] = nodes_coordiantes[i];
        nodes_text[i].style.transform = `translate(${x-ox}px, ${y-oy}px)`;
    }

    for (let i = 0; i < edges.length; i++) {
        edges[i].style.stroke = "gray";
        edges[i].style.strokeWidth = 1;
    }

    for (let g = 0; g < data.iterations[iteration].applied_gates.length; g++) {
        const gate = data.iterations[iteration].applied_gates[g];
        if (gate.length == 2) {
            const [p1, p2] = gate
            const e = edge_to_id[[p1, p2]]
            edges[e].style.stroke = "green";
            edges[e].style.strokeWidth = 5;
        }
    }

    for (let o = 0; o < data.iterations[iteration].applied_ops.length; o++) {
        const op = data.iterations[iteration].applied_ops[o];
        let color = "red";
        if (op.length == 3) {
            color = "blue";
        } else if (op.length == 4) {
            color = "purple";
        }
        for (let i = 0; i < op.length - 1; i++) {
            const e = edge_to_id[[op[i], op[i+1]]]
            edges[e].style.stroke = color;
            edges[e].style.strokeWidth = 5;
        }
    }

    // Need edges
    for (let i = 0; i < needed_edges.length; i++) {
        needed_edges[i].style.opacity = 0;
    }

    for (let i = 0; i < data.iterations[iteration].needed_paths.length; i++) {
        const path = data.iterations[iteration].needed_paths[i];
        console.log(path);
        for (let j = 0; j < path.length - 1; j++) {
            const e = edge_to_id[[path[j], path[j+1]]];
            needed_edges[e].style.opacity = 1;
        }
    }


    // Circuit

    for (let i = 0; i < gates.length ; i++) {
        gates[i].style.opacity = 0.1;
    }

    for (let i = 0; i < dependencies.length; i++) {
        dependencies[i].style.opacity = 0.1;
    }

    for (let i = 0; i < data.iterations[iteration].remaining_nodes.length ; i++) {
        const j = data.iterations[iteration].remaining_nodes[i];
        gates[j].style.opacity = 0.4;

        for (let k = 0; k < gate_to_edge_ids[j].length; k++) {
            const e = gate_to_edge_ids[j][k];
            dependencies[e].style.opacity = 0.4;
        }
    }

    for (let i = 0; i < data.iterations[iteration].front.length; i++) {
        const j = data.iterations[iteration].front[i];
        gates[j].style.opacity = 1;
    }


    // Candidate list
    candidate_list = document.getElementById('candidate-list');

    let ops = data.iterations[iteration].candidate_ops;
    let scores = data.iterations[iteration].candidate_ops_scores;
    let front_scores = data.iterations[iteration].candidate_ops_front_scores;
    let future_scores = data.iterations[iteration].candidate_ops_future_scores;
    console.log(scores)


    for (let i = 1; i < candidate_list.children.length; i++) {
        candidate_list.children[i].style.opacity = 0;
    }

    for (let i = 0; i < ops.length; i++) {
        const op = ops[i];
        
        let li;
        if (i < candidate_list.children.length - 1) {
            li = candidate_list.children[i + 1];
            li.style.opacity = 1;
        } else {
            li = document.createElement('li');
            const candidate_div = document.createElement('div');
            li.appendChild(candidate_div);
            const type_div = document.createElement('div');
            li.appendChild(type_div);
            const score_div = document.createElement('div');
            li.appendChild(score_div);
            const front_score_div = document.createElement('div');
            li.appendChild(front_score_div);
            const future_score_div = document.createElement('div');
            li.appendChild(future_score_div);

            li.addEventListener('mouseover', (event) => {
                const op = JSON.parse(event.currentTarget.dataset.op);
                let color = "black";
                for (let i = 0; i < op.length - 1; i++) {
                    const e = edge_to_id[[op[i], op[i+1]]]
                    edges[e].dataset.stroke = edges[e].style.stroke;
                    edges[e].dataset.strokeWidth = edges[e].style.strokeWidth;
                    edges[e].style.stroke = color;
                    edges[e].style.strokeWidth = 5;
                }
            });

            li.addEventListener('mouseout', (event) => {
                const op = JSON.parse(event.currentTarget.dataset.op);
                for (let i = 0; i < op.length - 1; i++) {
                    const e = edge_to_id[[op[i], op[i+1]]]
                    edges[e].style.stroke = edges[e].dataset.stroke;
                    edges[e].style.strokeWidth = edges[e].dataset.strokeWidth;
                }
            });

            candidate_list.appendChild(li);
        }

        li.dataset.op = JSON.stringify(op);        
        li.children[0].textContent = op;
        if (op.length == 2) {
            li.children[1].textContent = "Swap";
            li.children[1].classList.add('swap');
        } else if (op.length == 3) {
            li.children[1].textContent = "Teleport";
            li.children[1].classList.add('teleport');
        } else {
            li.children[1].textContent = "Telegate";
            li.children[1].classList.add('telegate');
        }
        li.children[2].textContent = scores[i].toFixed(3);
        li.children[3].textContent = front_scores[i].toFixed(3);
        li.children[4].textContent = future_scores[i].toFixed(3);
    }

    deadlock = document.getElementById('deadlock');
    if (data.iterations[iteration].solving_deadlock) {
        deadlock.style.opacity = 1;
    } else {
        deadlock.style.opacity = 0;
    }
}

function setupScenes() {
    console.log(data);

    nodes = [];
    edges = [];

    let svg = document.querySelector('#layout-scene svg');

    arch_data = data.architecture;
    const num_qubits = arch_data.node_positions.length;
    const num_virt_qubits = data.circuit.num_qubits;
    colors = generateNiceColors(num_virt_qubits);

    // Qubits
    const min_qubit_x = Math.min(...arch_data.node_positions.map(x => parseFloat(x[0])));
    const min_qubit_y = Math.min(...arch_data.node_positions.map(x => parseFloat(x[1])));


    for (let p = 0; p < num_qubits; p++) {
        const x = (arch_data.node_positions[p][0] - min_qubit_x + 0.2) * POS_SCALE;
        const y = (arch_data.node_positions[p][1] - min_qubit_y + 0.2) * POS_SCALE; 

        let newQubit = document.createElementNS("http://www.w3.org/2000/svg", 'circle');
        newQubit.setAttribute("cx", x); 
        newQubit.setAttribute("cy", y);
        newQubit.setAttribute("r", 7);
        newQubit.style.fill = "white";
        newQubit.style.stroke = "black";
        newQubit.style.strokeWidth = "1";
        newQubitTitle = document.createElementNS("http://www.w3.org/2000/svg", 'title');
        newQubitTitle.textContent = p;
        newQubit.appendChild(newQubitTitle);
        nodes.push(newQubit);
        nodes_coordiantes.push([x, y])

        const newQubitLabel = document.createElementNS("http://www.w3.org/2000/svg", 'text');
        newQubitLabel.setAttribute("x", x);
        newQubitLabel.setAttribute("y", y);
        newQubitLabel.setAttribute("text-anchor", "middle");
        newQubitLabel.setAttribute("dominant-baseline", "middle");
        newQubitLabel.textContent = p;
        newQubitLabel.style.opacity = "0";
        nodes_text.push(newQubitLabel);
    }

    // Edges
    for (let e = 0; e < arch_data.edges.length; e++) {
        let edge = arch_data.edges[e];
        let q1 = edge[0];
        let q2 = edge[1];
        let [x1, y1] = nodes_coordiantes[q1];
        let [x2, y2] = nodes_coordiantes[q2];

        let newEdge = document.createElementNS("http://www.w3.org/2000/svg", 'line');
        newEdge.setAttribute("x1", x1); //Set x1 coordinate
        newEdge.setAttribute("y1", y1); //Set y1 coordinate
        newEdge.setAttribute("x2", x2); //Set x2 coordinate
        newEdge.setAttribute("y2", y2); //Set y2 coordinate
        newEdge.style.stroke = "gray"; //Set stroke colour
        newEdge.style.strokeWidth = "1"; //Set stroke width
        edges.push(newEdge);

        let newNeededEdge = document.createElementNS("http://www.w3.org/2000/svg", 'line');
        newNeededEdge.setAttribute("x1", x1); //Set x1 coordinate
        newNeededEdge.setAttribute("y1", y1); //Set y1 coordinate
        newNeededEdge.setAttribute("x2", x2); //Set x2 coordinate
        newNeededEdge.setAttribute("y2", y2); //Set y2 coordinate
        newNeededEdge.style.stroke = "pink";
        newNeededEdge.style.strokeWidth = "6"; 
        newNeededEdge.style.strokeDasharray = "1.5 1.5";
        newNeededEdge.style.opacity = "0";
        needed_edges.push(newNeededEdge);

        edge_to_id[[q1, q2]] = e;
        edge_to_id[[q2, q1]] = e;
    }

    // TP-edges
    for (let r = 0; r < arch_data.teleport_edges.length; r++) {
        let edge = arch_data.teleport_edges[r];
        let q1 = edge[0];
        let q2 = edge[1];
        let [x1, y1] = nodes_coordiantes[q1];
        let [x2, y2] = nodes_coordiantes[q2];

        let newEdge = document.createElementNS("http://www.w3.org/2000/svg", 'path');
        newEdge.setAttribute("d", generateSinusoidPath(x1, y1, x2, y2));
        newEdge.setAttribute("fill", "none");
        newEdge.setAttribute("stroke", "gray");
        newEdge.setAttribute("stroke-width", "1");
        edges.push(newEdge);


        let newNeededEdge = document.createElementNS("http://www.w3.org/2000/svg", 'line');
        newNeededEdge.setAttribute("x1", x1); //Set x1 coordinate
        newNeededEdge.setAttribute("y1", y1); //Set y1 coordinate
        newNeededEdge.setAttribute("x2", x2); //Set x2 coordinate
        newNeededEdge.setAttribute("y2", y2); //Set y2 coordinate
        newNeededEdge.style.stroke = "pink";
        newNeededEdge.style.strokeWidth = "6"; 
        newNeededEdge.style.strokeDasharray = "1.5 1.5";
        newNeededEdge.style.opacity = "0";
        needed_edges.push(newNeededEdge);

        edge_to_id[[q1, q2]] = r + arch_data.edges.length;
        edge_to_id[[q2, q1]] = r + arch_data.edges.length;
    }


    // Add edges
    for (let i = 0; i < needed_edges.length; i++) {
        svg.appendChild(needed_edges[i]);
    }
    for (let i = 0; i < edges.length; i++) {
        svg.appendChild(edges[i]);
    }
    // Add qubits
    for (let i = 0; i < nodes.length; i++) {
        const [x,y] = nodes_coordiantes[i];
        let place = document.createElementNS("http://www.w3.org/2000/svg", 'circle');
        place.setAttribute("cx", x); //Set x coordinate
        place.setAttribute("cy", y); //Set y coordinate
        place.setAttribute("r", 3); //Set radius
        place.style.fill = "gray"; //Set fill colour
        svg.appendChild(place);        
    }

    for (let i = 0; i < nodes.length; i++) {
        svg.appendChild(nodes[i]);
        svg.appendChild(nodes_text[i]);
    }

    cir_data = data.circuit;
    console.log(cir_data);

    cir_svg = document.querySelector('#dag-scene svg');
    // Gates
    min_node_x = Math.min(...cir_data.node_positions.map(x => parseFloat(x[0])));
    min_node_y = Math.min(...cir_data.node_positions.map(x => parseFloat(x[1])));

    let gate_coordinates = [];

    for (let g = 0; g < cir_data.gates.length; g++) {
        gate_group = document.createElementNS("http://www.w3.org/2000/svg", 'g');
        let [x, y] = cir_data.node_positions[g];
        x = ((parseFloat(x) - min_node_x + 0.03) * POS_SCALE_CIR) * X_SCALE_CIR;
        y = ((parseFloat(y) - min_node_y + 0.03) * POS_SCALE_CIR) * Y_SCALE_CIR;
        let gate = cir_data.gates[g];

        const v1 = gate[0];
        const rect1 = document.createElementNS("http://www.w3.org/2000/svg", 'rect');
        rect1.setAttribute("x", x - 0.01 * POS_SCALE_CIR); //Set x coordinate
        rect1.setAttribute("y", y - 0.01 * POS_SCALE_CIR); //Set y coordinate
        rect1.setAttribute("width", 0.02 * POS_SCALE_CIR); //Set width
        rect1.setAttribute("height", 0.01 * POS_SCALE_CIR); //Set height
        rect1.style.fill = colors[v1];
        rect1.style.stroke = "black";
        rect1.style.strokeWidth = "1";

        const gateLabel1 = document.createElementNS("http://www.w3.org/2000/svg", 'text');
        gateLabel1.setAttribute("x", x);
        gateLabel1.setAttribute("y", y - 0.005 * POS_SCALE_CIR);
        gateLabel1.setAttribute("text-anchor", "middle");
        gateLabel1.setAttribute("dominant-baseline", "middle");
        gateLabel1.textContent = v1;

        if (gate.length == 2) {
            const v2 = gate[1];
            const rect2 = document.createElementNS("http://www.w3.org/2000/svg", 'rect');
            rect2.setAttribute("x", x - 0.01 * POS_SCALE_CIR); //Set x coordinate
            rect2.setAttribute("y", y ); //Set y coordinate
            rect2.setAttribute("width", 0.02 * POS_SCALE_CIR); //Set width
            rect2.setAttribute("height", 0.01 * POS_SCALE_CIR); //Set height
            rect2.style.fill = colors[v2];
            rect2.style.stroke = "black";
            rect2.style.strokeWidth = "1";
            gate_group.appendChild(rect2);

            const gateLabel2 = document.createElementNS("http://www.w3.org/2000/svg", 'text');
            gateLabel2.setAttribute("x", x);
            gateLabel2.setAttribute("y", y + 0.005 * POS_SCALE_CIR);
            gateLabel2.setAttribute("text-anchor", "middle");
            gateLabel2.setAttribute("dominant-baseline", "middle");
            gateLabel2.textContent = v2;
            gate_group.appendChild(gateLabel2);
        } else {
            rect1.setAttribute("y",  y - 0.005 * POS_SCALE_CIR)
            gateLabel1.setAttribute("y", y)
        }
        gate_group.appendChild(rect1);
        gate_group.appendChild(gateLabel1);


        gate_group.style.opacity = 0.4;

        gates.push(gate_group);
        gate_coordinates.push([x, y]);
        gate_to_edge_ids.push([]);
    }

    // Dependencies
    for (let d = 0; d < cir_data.dag.length; d++) {
        let [u, v] = cir_data.dag[d];
        let [x1, y1] = gate_coordinates[u];
        let [x2, y2] = gate_coordinates[v];

        let newEdge = document.createElementNS("http://www.w3.org/2000/svg", 'line');
        newEdge.setAttribute("x1", x1 + 0.01 * POS_SCALE_CIR); //Set x1 coordinate
        newEdge.setAttribute("y1", y1); //Set y1 coordinate
        newEdge.setAttribute("x2", x2 - 0.01 * POS_SCALE_CIR); //Set x2 coordinate
        newEdge.setAttribute("y2", y2); //Set y2 coordinate
        newEdge.style.stroke = "black"; //Set stroke colour
        newEdge.style.strokeWidth = "1"; //Set stroke width
        newEdge.style.opacity = 0.4;
        dependencies.push(newEdge);
        gate_to_edge_ids[u].push(d);
    }

    for (let i = 0; i < dependencies.length; i++) {
        cir_svg.appendChild(dependencies[i]);
    }

    for (let i = 0; i < gates.length; i++) {
        cir_svg.appendChild(gates[i]);
    }

    // Slider

    let slider = document.querySelector('#iteration-slider')
    slider.max = data.iterations.length - 1
    slider.min = 0
    slider.value = 0
    console.log('Iterations', data.iterations.length)

    slider.addEventListener('change', (event) => {
        let iteration = event.target.value
        console.log('Iteration', iteration)
        renderIteration(iteration)
    })

}

function retrieveData() {
    fetch('/data.json')
        .then(response => response.json())
        .then(retrieved_data => {
            data = retrieved_data
            setupScenes()
        })
}


function generateSinusoidPath(x1, y1, x2, y2, wavelength = 10, amplitude = 3) {
    const dx = x2 - x1;
    const dy = y2 - y1;
    const distance = Math.sqrt(dx * dx + dy * dy);

    const numPoints = Math.max(50, Math.ceil(distance / (wavelength / 10)));
    let pathData = `M ${x1} ${y1}`;

    const ux = dx / distance;
    const uy = dy / distance;

    const vx = -uy;
    const vy = ux;
    for (let i = 1; i <= numPoints; i++) {
        const t = i / numPoints;
        const distTraveled = t * distance;
        const waveOffset = amplitude * Math.sin(2 * Math.PI * distTraveled / wavelength);
        const baseX = x1 + t * dx;
        const baseY = y1 + t * dy;
        const x = baseX + vx * waveOffset;
        const y = baseY + vy * waveOffset;
        pathData += ` L ${x} ${y}`;
    }
    return pathData;
}


function generateNiceColors(n) {
    const colors = [];
    const goldenRatio = 0.618033988749895;
    let hue = Math.random();

    for (let i = 0; i < n; i++) {
        // Utilizzo del rapporto aureo per una distribuzione armoniosa dei colori
        hue = (hue + goldenRatio) % 1;

        // Saturazione e luminosità fisse per colori vivaci ma piacevoli
        const saturation = 0.5 + Math.random() * 0.2; // 0.5-0.7
        const lightness = 0.6 + Math.random() * 0.2;  // 0.4-0.6

        // Conversione da HSL a esadecimale
        const h = Math.floor(hue * 360);
        const s = Math.floor(saturation * 100);
        const l = Math.floor(lightness * 100);

        colors.push(`hsl(${h}, ${s}%, ${l}%)`);
    }

    return colors;
}

retrieveData()
enablePanZoom()


// Splitter

function startDrag() {
    console.log('down');
    glass.style = 'display: block;';
    glass.addEventListener('mousemove', drag, false);
}

function endDrag() {
    console.log('up');
    glass.removeEventListener('mousemove', drag, false);
    glass.style = '';
}

function drag(event) {
    console.log('move');
    var splitter = getSplitter();
    var panel = document.querySelector('.dag-section');
    var currentWidth = panel.offsetWidth;
    var currentLeft = panel.offsetLeft;
    panel.style.width = (currentWidth - (event.clientX - currentLeft)) + "px";
}

function getSplitter() {
    return document.getElementById('splitter');
}

var con = document.getElementById('container');
var splitter = document.createElement('div');
var glass = document.getElementById('glass');
splitter.className = 'splitter';
splitter.id = 'splitter';
con.insertBefore(splitter, con.lastElementChild);
splitter.addEventListener('mousedown', startDrag, false);
glass.addEventListener('mouseup', endDrag, false);




for (let e of document.querySelectorAll('input[type="range"].slider-progress')) {
    e.style.setProperty('--value', e.value);
    e.style.setProperty('--min', e.min == '' ? '0' : e.min);
    e.style.setProperty('--max', e.max == '' ? '100' : e.max);
    e.addEventListener('change', () => {
        e.style.setProperty('--min', e.min == '' ? '0' : e.min);
        e.style.setProperty('--max', e.max == '' ? '100' : e.max);
        e.style.setProperty('--value', e.value)
    });
  }




  // Slider play


  // Auto Play
  const slider = document.getElementById('iteration-slider');
  const playPauseBtn = document.getElementById('playPause');
  const intervalInput = document.getElementById('interval');
  let interval = parseFloat(intervalInput.value) * 1000;
  let autoPlay = null;
  let isPlaying = false;

  function startAutoPlay() {
      stopAutoPlay();
      autoPlay = setInterval(() => {
          slider.stepUp();
          slider.dispatchEvent(new Event("change"));
          if (slider.value == slider.max) stopAutoPlay();
      }, interval);
      isPlaying = true;
      playPauseBtn.textContent = '❚❚';
  }

  function stopAutoPlay() {
      clearInterval(autoPlay);
      isPlaying = false;
      playPauseBtn.textContent = '▶';
  }

  playPauseBtn.addEventListener('click', () => {
      isPlaying ? stopAutoPlay() : startAutoPlay();
  });

  intervalInput.addEventListener('input', () => {
      interval = parseFloat(intervalInput.value) * 1000;
      if (isPlaying) startAutoPlay();
  });

  slider.addEventListener('input', () => {
      stopAutoPlay();
      slider.dispatchEvent(new Event("change"));
  });

  document.getElementById('prev-it').addEventListener('click', () => {
      slider.stepDown();
      slider.dispatchEvent(new Event("change"));
  });
  document.getElementById('next-it').addEventListener('click', () => {
      slider.stepUp();
      slider.dispatchEvent(new Event("change"));
  });


document.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowRight') {
        stopAutoPlay();
        slider.stepUp();
        slider.dispatchEvent(new Event("change"));
    } else if (event.key === 'ArrowLeft') {
        stopAutoPlay();
        slider.stepDown();
        slider.dispatchEvent(new Event("change"));
    } else if (event.key === ' ') {
        isPlaying ? stopAutoPlay() : startAutoPlay();
    }
});