#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../test_helpers.hpp"
#include "whis/compiler.hpp"

struct SpecTestCase {
  std::string section_meta;
  std::string source_code;
};

int main() {
  print_suite_header("Whis Parser Grammar Rules");

  const std::vector<SpecTestCase> test_cases = {
      {"§2.3 Unit Scales and Literals",
       "let structural_length = 45 cm;\n"
       "let aircraft_speed = 900 km/h;\n"
       "let target_mass = 4.2 mg;"},

      {"§5.1 Declarative Primitives for Dynamic Entities",
       "create Projectile {\n"
       "    shape: circle,\n"
       "    radius: 0.5 m,\n"
       "    colour: blue,\n"
       "    mass: 14 kg,\n"
       "    position: (0 m, 25 m),\n"
       "    velocity: (12 m/s, 5 m/s),\n"
       "    charge: 0.0 C\n"
       "}"},

      {"§5.1 Declarative Primitives for Static Coefficient Surfaces",
       "create Ground {\n"
       "    shape: plane,\n"
       "    normal: (0, 1),\n"
       "    position: (0 m, 0 m),\n"
       "    friction: 0.35,\n"
       "    bounciness: 0.65\n"
       "}"},

      {"§5.2 Environmental Modifiers",
       "enable gravity(9.81 m/s^2);\n"
       "enable air_resistance(density: 1.225 kg/m^3, drag_coefficient: 0.47);\n"
       "enable electrostatic_field(intensity: (0 V/m, -100 V/m));"},

      {"§6.1 Control Flow & Functions",
       "fn calculate_gravity(m1: kg, m2: kg, distance: m) -> N {\n"
       "    let G = 6.6743e-11 N*m^2/kg^2;\n"
       "    return G * (m1 * m2) / (distance ^ 2);\n"
       "}\n"
       "while (system.time < 10 s) {\n"
       "    let separation = target.position - source.position;\n"
       "    if (magnitude(separation) < 1.0 m) {\n"
       "        break;\n"
       "    }\n"
       "}"},

      {"§6.2 Custom Differential Solvers for Step Routines",
       "step system(dt: s) {\n"
       "    for obj in system.entities {\n"
       "        if (obj.is_static) { continue; }\n"
       "        let net_force = obj.forces + "
       "system.environmental_forces_for(obj);\n"
       "        let acceleration = net_force / obj.mass;\n"
       "        obj.velocity = obj.velocity + (acceleration * dt);\n"
       "        obj.position = obj.position + (obj.velocity * dt);\n"
       "    }\n"
       "}"}};

  for (const auto& tc : test_cases) {
    print_case_start(tc.section_meta);
    auto result = whis::compiler::generate_ast(tc.source_code);

    WHIS_ASSERT(result.success, "Syntax rejected: " + result.error_log);
    print_case_success();
  }
  return 0;
}