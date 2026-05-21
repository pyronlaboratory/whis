# Language Specification: Whis `v0.1-Draft`

A Domain-Specific Language for Physics Simulation, Education, and Analysis

## 1. Design Philosophy

Whis is an open-source, domain-specific programming language designed to bridge the gap between whiteboard physics and high-performance bare-metal execution.
The design rests on three non-negotiable principles:

- **Syntax as Notation**: Writing Whis should feel identical to writing equations and configuring structural diagrams on a physics whiteboard. Syntax friction like structural boilerplate, explicit templates (`Vector3D<float64>`), and manual unit conversions are treated as language defects.
- **Compile-Time Physical Integrity**: The language treats physical dimensions as a fundamental aspect of the type system. Dimensional discrepancies (e.g., adding mass to velocity) are caught during compilation rather than failing during execution.
- **Dual Engine Runtime**: Whis operates natively as a hybrid language. It features a declarative Configuration Phase to model space, boundaries, and actors, and an imperative Evaluation Phase to compute arbitrary equations, vector fields, and custom numerical methods.

## 2. Type System & Dimensional Validation

Whis implements a strong, statically typed, structurally inferenced type system where scalars and vectors are natively bound to physical dimensions.

### 2.1 Dimensional Mechanics (SI Base Tracking)

Every mathematical value in Whis is internally tracked via a 7-element vector of integers representing the exponents of the fundamental SI base units:

$$\text{Dimension} = [M, L, T, I, \Theta, N, J]$$

Where the indices correspond to:

1. $M$: Mass (kilogram, kg)
2. $L$: Length (meter, m)
3. $T$: Time (second, s)
4. $I$: Electric Current (ampere, A)
5. $\Theta$: Thermodynamic Temperature (kelvin, K)
6. $N$: Amount of Substance (mole, mol)
7. $J$: Luminous Intensity (candela, cd)

#### Primary Derived Dimension Matrix

The runtime compiler maps primitive expressions to their dimensional exponents:

| Derived Unit  | Symbol  | Base Formula                | Dimension Array [M,L,T,I,Θ,N,J] |
| :------------ | :------ | :-------------------------- | :------------------------------ |
| Dimensionless | —       | $1$                         | $[0, 0, 0, 0, 0, 0, 0]$         |
| Length        | m       | $m$                         | $[0, 1, 0, 0, 0, 0, 0]$         |
| Mass          | kg      | $kg$                        | $[1, 0, 0, 0, 0, 0, 0]$         |
| Time          | s       | $s$                         | $[0, 0, 1, 0, 0, 0, 0]$         |
| Velocity      | m/s     | $m \cdot s^{-1}$            | $[0, 1, -1, 0, 0, 0, 0]$        |
| Acceleration  | m/s$^2$ | $m \cdot s^{-2}$            | $[0, 1, -2, 0, 0, 0, 0]$        |
| Force         | N       | $kg \cdot m \cdot s^{-2}$   | $[1, 1, -2, 0, 0, 0, 0]$        |
| Energy        | J       | $kg \cdot m^2 \cdot s^{-2}$ | $[1, 2, -2, 0, 0, 0, 0]$        |

### 2.2 Algebraic Propagation Rules

During the abstract syntax tree (AST) evaluation pass, the compiler validates compound types using static dimensional arithmetic:

- **Addition / Subtraction**: $A \pm B \implies \text{Requires } \text{Dim}(A) \equiv \text{Dim}(B)$. The resulting dimension is $\text{Dim}(A)$.

- **Multiplication**: $A \cdot B \implies \text{Dim}(A) + \text{Dim}(B)$ (element-wise addition of exponent arrays).

- **Division**: $A / B \implies \text{Dim}(A) - \text{Dim}(B)$ (element-wise subtraction of exponent arrays).

- **Power / Roots**: $A^n \implies \text{Dim}(A) \cdot n$. Powers must resolve to a compile-time numeric constant. Transcendental functions ($\sin, \cos, \ln, \exp$) strictly require an operand with a fully dimensionless array $[0,0,0,0,0,0,0]$.

### 2.3 Unit Scales and Literals

Whis treats unit symbols as literal scaling suffixes. The compiler normalizes all suffixes down to baseline SI expressions during compilation.

```bash
let structural_length = 45 cm;  // Parsed as 45, Scaled to 0.45, Type: [0, 1, 0, 0, 0, 0, 0]
let aircraft_speed = 900 km/h;  // Parsed as 900, Scaled to 250.0, Type: [0, 1, -1, 0, 0, 0, 0]
let target_mass = 4.2 mg;       // Parsed as 4.2, Scaled to 4.2e-6, Type: [1, 0, 0, 0, 0, 0, 0]
```

## 3. Grammar & Concrete Syntax

Whis source files use UTF-8 strings. The language uses a newline-sensitive, clean-layout grammar where trailing semicolons are optional for structural blocks but required for statement separation within inline blocks.

### 3.1 Lexical Tokens

- **Keywords**: `create`, `let`, `enable`, `disable`, `show`, `hide`, `if`, `else`, `while`, `for`, `in`, `fn`, `step`, `state`.

- **Identifiers**: `[a-zA-Z\_][a-zA-Z0-9_]\_`

- **Numeric Literals**: `[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?`

- **Unit Suffixes**: `m`, `cm`, `mm`, `km`, `g`, `kg`, `mg`, `s`, `ms`, `min`, `h`, `N`, `J`, `W`, `Pa`, `V`, `A`, `C`, `rad`, `deg`.

### 3.2 Context-Free Grammar (EBNF Excerpt)

```bash
EBNF
Program         ::= Statement_
Statement       ::= CreateStmt | LetStmt | EnableStmt | ShowStmt | ControlStmt | AssignStmt
CreateStmt      ::= "create" Identifier "{" PropertyList "}"
PropertyList    ::= (Identifier ":" Expression ("," | Newline)?)_
LetStmt         ::= "let" Identifier ("=" Expression)?
EnableStmt      ::= ("enable" | "disable") Identifier ("(" ExpressionList ")")?
ShowStmt        ::= ("show" | "hide") Identifier ("(" ExpressionList ")")?
Expression      ::= Term (( "+" | "-" ) Term)_
Term            ::= Factor (( "_" | "/" ) Factor)_
Factor          ::= Primary ( "^" Primary )? Suffix?
Primary         ::= NumericLiteral | Identifier | VectorLiteral | "(" Expression ")"
VectorLiteral   ::= "(" Expression "," Expression ("," Expression)? ")"
Suffix          ::= Identifier ("/" Identifier)? ("^" NumericLiteral)?
```

## 4. Execution Model & Lifecycle

Whis executes code through a discrete two-stage pipeline designed to decouple initialization states from the high-frequency evaluation loops required by mathematical solvers.

```text
                     ┌───────────────────────┐
                     │  Source Compilation   │
                     └───────────────────────┘
                                │
                                ▼
        ┌──────────────────────────────────────────────┐
        │ 1. Structural Setup                          │
        │──────────────────────────────────────────────│
        │ Executes once                                │
        │                                              │
        │ Allocates entities and memory                │
        └──────────────────────────────────────────────┘
                                │
                                ▼
        ┌──────────────────────────────────────────────┐
        │ 2. Discrete Physics Loop                     │
        │──────────────────────────────────────────────│
        │ Runs indefinitely at frequency (f)           │
        │                                              │
        │  ┌────────────────────────────────────────┐  │
        │  │ State Tweak                            │──┼──► Injects real-time variable mutations
        │  └────────────────────────────────────────┘  │
        │                                              │
        │  ┌────────────────────────────────────────┐  │
        │  │ Integration                            │──┼──► Evaluates positions/velocities via RK4
        │  └────────────────────────────────────────┘  │
        │                                              │
        │  ┌────────────────────────────────────────┐  │
        │  │ Constraint / Collision                 │──┼──► Solves impulse manifolds and collisions
        │  └────────────────────────────────────────┘  │
        └──────────────────────────────────────────────┘
```

### 4.1 Phase 1: Structural Setup Phase

The virtual machine executes top-level declarative statements sequentially to construct the simulation framework.

- Memory allocations for objects, boundaries, physical properties, and vector fields are prioritized and pinned in layout blocks.

- System environments (e.g., global gravitational fields, fluid densities) are bound directly to the simulation pipeline.

### 4.2 Phase 2: The Discrete Physics Loop (The Tick)

Once the setup phase terminates, the execution loop takes absolute control. It maps every simulated step to a fixed or dynamic delta time interval ($dt$). Each tick executes the following nested steps sequentially:

- **State Tweak Application**: External thread-safe registers update values changed by user UI interactions (like moving a slider).

- **Explicit Integration Pass**: Kinematic variables pass through numerical solvers to compute updated predictive velocities and positional translations.

- **Constraint & Collision Resolution**: Positional boundaries and narrow-phase shape intersections compute velocity impulses and positional corrections across an iterative array of mathematical manifolds.

## 5. Declarative Scene System

The declarative syntax instantiates structural nodes inside the internal physics system registry. Objects represent real geometric primitives wrapped in physical states.

### 5.1 Object Primitives and Structural Layouts

Every instantiated object requires structural properties (shape, position) alongside physical markers (mass, charge, bounciness).

```bash
create Projectile {
    shape: circle,
    radius: 0.5 m,
    colour: blue,
    mass: 14 kg,
    position: (0 m, 25 m),
    velocity: (12 m/s, 5 m/s),
    charge: 0.0 C
}

create Ground {
    shape: plane,
    normal: (0, 1),
    position: (0 m, 0 m),
    friction: 0.35,
    bounciness: 0.65
}
```

### 5.2 Environmental Modifiers

Environmental modifiers inject global field behaviors directly into the execution phase without requiring manual iteration across object loops.

```bash
enable gravity(9.81 m/s^2);
enable air_resistance(density: 1.225 kg/m^3, drag_coefficient: 0.47);
enable electrostatic_field(intensity: (0 V/m, -100 V/m));
```

## 6. Imperative Logic & Numerical Methods

For university-level and specialized engineering use cases, Whis provides an imperative syntax layer capable of running arbitrary numerical models, handling vector transformations, and mutating custom simulation parameters.

### 6.1 Control Flow & Functions

Functions maintain strict compile-time signature verification for dimensions.

```bash
// Function computes gravitational attraction force between two masses
fn calculate_gravity(m1: kg, m2: kg, distance: m) -> N {
    let G = 6.6743e-11 N*m^2/kg^2;
    return G * (m1 \* m2) / (distance ^ 2);
}

// Inline conditional modification within execution updates
while (system.time < 10 s) {
    let separation = target.position - source.position;
    if (magnitude(separation) < 1.0 m) {
        break;
    }
}
```

### 6.2 Custom Differential Solvers (The `step` Block)

Users can override standard engine solvers by implementing explicit numerical integration routines via contextual `step` keywords.

```bash
// Overriding default integration with an explicit Modified Euler execution
step system(dt: s) {
    for obj in system.entities {
        if (obj.is_static) continue;

        // Compute net force summation registered from active components
        let net_force = obj.forces + system.environmental_forces_for(obj);
        let acceleration = net_force / obj.mass;

        // Mutate spatial states imperatively
        obj.velocity = obj.velocity + (acceleration * dt);
        obj.position = obj.position + (obj.velocity * dt);
    }
}
```

## 7. Interactive Visualization & Instrumentation Engine

Whis treats rendering diagnostics and instrumentation as a core part of its grammar, separating visual annotations from structural objects.

### 7.1 Visual Vector Overlays

The `show` mechanism establishes visual hooks that the execution engine uses to stream geometric overlay primitives directly to frontend rendering pipelines.

```bash
// Attaches dynamic vector arrows directly to spatial coordinate frames
show vectors(velocity, acceleration) on Projectile;
show fieldLines(electrostatic_field) resolution(20x20);
```

### 7.2 The Interactivity Interface (`tweak` / UI Bindings)

When the compiler encounters an unassigned initialized variable bound to an instrumentation constraint, it registers it as a dynamic tweakable parameter.

```bash
// Exposing variables to IDE slider generation pipelines
let push_force = tweak(10 N, range: 0 N .. 100 N);
let target_mass = tweak(5 kg, range: 1 kg .. 50 kg);

create ActiveBlock {
    shape: box,
    dimensions: (1 m, 1 m),
    mass: target_mass,
    forces: (push_force, 0 N)
}
```

## 8. Compiler & Virtual Machine Architecture

The Whis engine uses a two-pass optimizing compiler matched to an explicit register-based bytecode virtual machine designed for modern CPU execution layout optimizations.

### 8.1 Register-Based VM Instruction Set (Physics Variants)

The virtual machine drops general-purpose, stack-heavy execution paradigms. Instead, it utilizes specialized instructions designed for handling standard scalar and vector floating-point math packed closely together in memory.

| Opcode       | Operands                      | Description                                                         |
| :----------- | :---------------------------- | :------------------------------------------------------------------ |
| `LOAD_VEC`   | `R_dest, imm_x, imm_y, imm_z` | Loads a 3D vector primitive into register `R_dest`                  |
| `VADD`       | `R_dest, R_src1, R_src2`      | Performs vector addition calculation                                |
| `VMUL_SCL`   | `R_dest, R_src1, R_scalar`    | Scales vector `R_src1` by scalar register `R_scalar`                |
| `INTEG_VV`   | `R_pos, R_vel, R_acc, dt`     | Executes a single-cycle Velocity Verlet mathematical pass           |
| `POLL_TWEAK` | `R_dest, tweak_id`            | Reads atomic shared-memory boundaries for external slider mutations |

### 8.2 Memory Model (Structure-of-Arrays Topology)

To minimize CPU cache line misses during large multi-particle simulations, the virtual machine converts high-level object states down into memory-contiguous Structure-of-Arrays (SoA) blocks inside native C++ memory managers.

```
Object Oriented Representation (Bad for Cache Optimization):
[ [Pos_x, Pos_y, Vel_x, Vel_y, Mass], [Pos_x, Pos_y, Vel_x, Vel_y, Mass] ]

Whis Virtual Machine Layout (Contiguous Structure-of-Arrays):
Positions X: [ x0, x1, x2, x3, x4, x5, ... xN ]
Positions Y: [ y0, y1, y2, y3, y4, y5, ... yN ]
Velocities X: [ vx0, vx1, vx2, vx3, vx4, vx5, ... vxN ]
Velocities Y: [ vy0, vy1, vy2, vy3, vy4, vy5, ... vyN ]
Masses: [ m0, m1, m2, m3, m4, m5, ... mN ]
```

During execution loops, vector calculation loops stream through these arrays sequentially, allowing modern CPUs to leverage SIMD (Single Instruction, Multiple Data) execution lines for blazing-fast performance.

## 9. Error Specification Matrix

To support high school and undergraduate students, compilation and runtime errors avoid abstract compiler jargon. Instead, they frame errors within the context of physical laws.

| Error Code                | Class   | Message Template / Context                                                                           | Remediation Action Hint                                                                                            |
| :------------------------ | :------ | :--------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------- |
| `E_DIM_MISMATCH`          | Compile | Dimensional Analysis Violation: Cannot add `[Mass]` to `[Velocity]` in expression `'5 kg + 12 m/s'`. | Check equation derivation against dimensional tracking steps.                                                      |
| `E_UNIT_NON_TRANSCENDENT` | Compile | Mathematical Boundary Error: Arguments to `sin()` must be fully dimensionless. Received `[Length]`.  | Convert raw dimension components down to dimensionless relative ratios before evaluating transcendental functions. |
| `E_SINGULARITY_DIV`       | Runtime | Physical Singularity Detected: Division by zero calculated during force computation.                 | Wrap denominator parameters inside protective boundary clips or distance offsets.                                  |
| `E_STATE_OVERFLOW`        | Runtime | Kinematic Instability: Object speed exploded past safety limits due to integration step instability. | Switch system numerical methods to RK4 or reduce timestep values ($dt$).                                         |
