container list [
  value:location
  next:address:list
]

# result:address:list <- push x:location, in:address:list
recipe push [
  default-space:address:array:location <- new location:type, 30:literal
  x:location <- next-ingredient
  in:address:list <- next-ingredient
  result:address:list <- new list:type
  val:address:location <- get-address result:address:list/deref, value:offset
  val:address:location/deref <- copy x:location
  next:address:address:list <- get-address result:address:list/deref, next:offset
  next:address:address:list/deref <- copy in:address:list
  reply result:address:list
]

# result:location <- first in:address:list
recipe first [
  default-space:address:array:location <- new location:type, 30:literal
  in:address:list <- next-ingredient
  result:location <- get in:address:list/deref, value:offset
  reply result:location
]

# result:address:list <- rest in:address:list
recipe rest [
  default-space:address:array:location <- new location:type, 30:literal
  in:address:list <- next-ingredient
  result:address:list <- get in:address:list/deref, next:offset
  reply result:address:list
]

scenario list-handling [
  run [
#?     $start-tracing #? 1
    1:address:list <- copy 0:literal
    1:address:list <- push 3:literal, 1:address:list
    1:address:list <- push 4:literal, 1:address:list
    1:address:list <- push 5:literal, 1:address:list
    2:number <- first 1:address:list
    1:address:list <- rest 1:address:list
    3:number <- first 1:address:list
    1:address:list <- rest 1:address:list
    4:number <- first 1:address:list
    1:address:list <- rest 1:address:list
  ]
  memory-should-contain [
    1 <- 0  # empty to empty, dust to dust..
    2 <- 5
    3 <- 4
    4 <- 3
  ]
]