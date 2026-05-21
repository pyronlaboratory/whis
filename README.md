# Whis
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

An open-source, domain-specific programming language designed to bridge the gap between whiteboard physics and high-performance bare-metal execution.

The language treats physical dimensions ($[M, L, T, I, \Theta, N, J]$) as fundamental compile-time types, supports a dual declarative/imperative runtime model, and compiles down to a custom register-based VM utilizing a high-efficiency Structure-of-Arrays (SoA) memory topology for SIMD vector optimization.

## Key Features
* **Compile-Time Physical Integrity**: Automatic static dimensional analysis propagation at compile time.
* **Unit Scales as Suffix Literals**: Direct support for scientific literals normalizing to baseline SI values.
* **Dual-Engine Execution Loop**: Clean decoupling of a declarative *Structural Setup Phase* from a high-frequency, optimized *Discrete Physics Loop*.

## License
The `whis` language core and bindings are open-source software licensed under the **Apache License 2.0**. See the `LICENSE` file for full terms.
