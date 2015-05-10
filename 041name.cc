//: A big convenience high-level languages provide is the ability to name memory
//: locations. In mu, a transform called 'transform_names' provides this
//: convenience.

:(scenario convert_names)
recipe main [
  x:integer <- copy 0:literal
]
+name: assign x 1
+run: instruction main/0
+mem: storing 0 in location 1

:(scenario convert_names_warns)
% Hide_warnings = true;
recipe main [
  x:integer <- copy y:integer
]
+warn: use before set: y in main

:(after "int main")
  Transform.push_back(transform_names);

:(before "End Globals")
map<recipe_number, map<string, index_t> > Name;
:(after "Clear Other State For recently_added_recipes")
for (index_t i = 0; i < recently_added_recipes.size(); ++i) {
  Name.erase(recently_added_recipes.at(i));
}

:(code)
void transform_names(const recipe_number r) {
  bool names_used = false;
  bool numeric_locations_used = false;
  map<string, index_t>& names = Name[r];
  // store the indices 'used' so far in the map
  index_t& curr_idx = names[""];
  ++curr_idx;  // avoid using index 0, benign skip in some other cases
  for (index_t i = 0; i < Recipe[r].steps.size(); ++i) {
    instruction& inst = Recipe[r].steps.at(i);
    // Per-recipe Transforms
    // map names to addresses
    for (index_t in = 0; in < inst.ingredients.size(); ++in) {
      if (is_numeric_location(inst.ingredients.at(in))) numeric_locations_used = true;
      if (is_named_location(inst.ingredients.at(in))) names_used = true;
      if (disqualified(inst.ingredients.at(in))) continue;
      if (!already_transformed(inst.ingredients.at(in), names)) {
        raise << "use before set: " << inst.ingredients.at(in).name << " in " << Recipe[r].name << '\n';
      }
      inst.ingredients.at(in).set_value(lookup_name(inst.ingredients.at(in), r));
    }
    for (index_t out = 0; out < inst.products.size(); ++out) {
      if (is_numeric_location(inst.products.at(out))) numeric_locations_used = true;
      if (is_named_location(inst.products.at(out))) names_used = true;
      if (disqualified(inst.products.at(out))) continue;
      if (names.find(inst.products.at(out).name) == names.end()) {
        trace("name") << "assign " << inst.products.at(out).name << " " << curr_idx;
        names[inst.products.at(out).name] = curr_idx;
        curr_idx += size_of(inst.products.at(out));
      }
      inst.products.at(out).set_value(lookup_name(inst.products.at(out), r));
    }
  }
  if (names_used && numeric_locations_used)
    raise << "mixing variable names and numeric addresses in " << Recipe[r].name << '\n';
}

bool disqualified(/*mutable*/ reagent& x) {
  if (x.types.empty())
    raise << "missing type in " << x.to_string() << '\n';
  assert(!x.types.empty());
  if (is_raw(x)) return true;
  if (isa_literal(x)) return true;
  if (is_number(x.name)) return true;
  if (x.name == "default-space")
    x.initialized = true;
  if (x.initialized) return true;
  return false;
}

bool already_transformed(const reagent& r, const map<string, index_t>& names) {
  return names.find(r.name) != names.end();
}

index_t lookup_name(const reagent& r, const recipe_number default_recipe) {
  return Name[default_recipe][r.name];
}

type_number skip_addresses(const vector<type_number>& types) {
  for (index_t i = 0; i < types.size(); ++i) {
    if (types.at(i) != Type_number["address"]) return types.at(i);
  }
  raise << "expected a container" << '\n' << die();
  return -1;
}

int find_element_name(const type_number t, const string& name) {
  const type_info& container = Type[t];
//?   cout << "looking for element " << name << " in type " << container.name << " with " << container.element_names.size() << " elements\n"; //? 1
  for (index_t i = 0; i < container.element_names.size(); ++i) {
    if (container.element_names.at(i) == name) return i;
  }
  raise << "unknown element " << name << " in container " << t << '\n' << die();
  return -1;
}

bool is_numeric_location(const reagent& x) {
  if (isa_literal(x)) return false;
  if (is_raw(x)) return false;
  if (x.name == "0") return false;  // used for chaining lexical scopes
  return is_number(x.name);
}

bool is_named_location(const reagent& x) {
  if (isa_literal(x)) return false;
  if (is_raw(x)) return false;
  if (is_special_name(x.name)) return false;
  return !is_number(x.name);
}

bool is_raw(const reagent& r) {
  for (index_t i = /*skip value+type*/1; i < r.properties.size(); ++i) {
    if (r.properties.at(i).first == "raw") return true;
  }
  return false;
}

bool is_special_name(const string& s) {
  if (s == "_") return true;
  // lexical scopes
  if (s == "default-space") return true;
  if (s == "0") return true;
  // tests will use these in later layers even though tests will mostly use numeric addresses
  if (s == "screen") return true;
  if (s == "keyboard") return true;
  return false;
}

:(scenario convert_names_passes_dummy)
# _ is just a dummy result that never gets consumed
recipe main [
  _, x:integer <- copy 0:literal, 1:literal
]
+name: assign x 1
-name: assign _ 1

//: one reserved word that we'll need later
:(scenario convert_names_passes_default_space)
recipe main [
  default-space:integer, x:integer <- copy 0:literal, 1:literal
]
+name: assign x 1
-name: assign default-space 1

//: an escape hatch to suppress name conversion that we'll use later
:(scenario convert_names_passes_raw)
recipe main [
  x:integer/raw <- copy 0:literal
]
-name: assign x 1

:(scenario convert_names_warns_when_mixing_names_and_numeric_locations)
% Hide_warnings = true;
recipe main [
  x:integer <- copy 1:integer
]
+warn: mixing variable names and numeric addresses in main

:(scenario convert_names_warns_when_mixing_names_and_numeric_locations2)
% Hide_warnings = true;
recipe main [
  x:integer <- copy 1:literal
  1:integer <- copy x:integer
]
+warn: mixing variable names and numeric addresses in main

:(scenario convert_names_does_not_warn_when_mixing_names_and_raw_locations)
% Hide_warnings = true;
recipe main [
  x:integer <- copy 1:integer/raw
]
-warn: mixing variable names and numeric addresses in main

:(scenario convert_names_does_not_warn_when_mixing_names_and_literals)
% Hide_warnings = true;
recipe main [
  x:integer <- copy 1:literal
]
-warn: mixing variable names and numeric addresses in main

:(scenario convert_names_does_not_warn_when_mixing_special_names_and_numeric_locations)
% Hide_warnings = true;
recipe main [
  screen:integer <- copy 1:integer
]
-warn: mixing variable names and numeric addresses in main

//:: Support element names for containers in 'get' and 'get-address'.

//: update our running example container for the next test
:(before "End Mu Types Initialization")
Type[point].element_names.push_back("x");
Type[point].element_names.push_back("y");
:(scenario convert_names_transforms_container_elements)
recipe main [
  p:address:point <- copy 0:literal  # unsafe
  a:integer <- get p:address:point/deref, y:offset
  b:integer <- get p:address:point/deref, x:offset
]
+name: element y of type point is at offset 1
+name: element x of type point is at offset 0

:(after "Per-recipe Transforms")
// replace element names of containers with offsets
if (inst.operation == Recipe_number["get"]
    || inst.operation == Recipe_number["get-address"]) {
  // at least 2 args, and second arg is offset
  assert(inst.ingredients.size() >= 2);
//?   cout << inst.ingredients.at(1).to_string() << '\n'; //? 1
  assert(isa_literal(inst.ingredients.at(1)));
  if (inst.ingredients.at(1).name.find_first_not_of("0123456789") == string::npos) continue;
  // since first non-address in base type must be a container, we don't have to canonize
  type_number base_type = skip_addresses(inst.ingredients.at(0).types);
  inst.ingredients.at(1).set_value(find_element_name(base_type, inst.ingredients.at(1).name));
  trace("name") << "element " << inst.ingredients.at(1).name << " of type " << Type[base_type].name << " is at offset " << inst.ingredients.at(1).value;
}

//: this test is actually illegal so can't call run
:(scenarios transform)
:(scenario convert_names_handles_containers)
recipe main [
  a:point <- copy 0:literal
  b:integer <- copy 0:literal
]
+name: assign a 1
+name: assign b 3

//:: Support variant names for exclusive containers in 'maybe-convert'.

:(scenarios run)
:(scenario maybe_convert_named)
recipe main [
  12:integer <- copy 1:literal
  13:integer <- copy 35:literal
  14:integer <- copy 36:literal
  20:address:point <- maybe-convert 12:integer-or-point, p:variant
]
+name: variant p of type integer-or-point has tag 1
+mem: storing 13 in location 20

:(after "Per-recipe Transforms")
// convert variant names of exclusive containers
if (inst.operation == Recipe_number["maybe-convert"]) {
  // at least 2 args, and second arg is offset
  assert(inst.ingredients.size() >= 2);
  assert(isa_literal(inst.ingredients.at(1)));
  if (inst.ingredients.at(1).name.find_first_not_of("0123456789") == string::npos) continue;
  // since first non-address in base type must be an exclusive container, we don't have to canonize
  type_number base_type = skip_addresses(inst.ingredients.at(0).types);
  inst.ingredients.at(1).set_value(find_element_name(base_type, inst.ingredients.at(1).name));
  trace("name") << "variant " << inst.ingredients.at(1).name << " of type " << Type[base_type].name << " has tag " << inst.ingredients.at(1).value;
}