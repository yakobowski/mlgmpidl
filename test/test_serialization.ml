(*
 * Test program for serialization support in mlgmpidl
 *)

open Printf

external internal_init_custom_ops : unit -> unit = "mlgmpidl_init_custom_ops"

let test_mpz () =
  print_endline "Testing Mpz serialization...";
  let z = Mpz.init () in
  Mpz.set_str z "123456789012345678901234567890" ~base:10;
  let original = Mpz.to_string z in
  
  (* Serialize to string *)
  let serialized = Marshal.to_string z [] in
  
  print_endline "Testing Mpz deserialization...";
  (* Deserialize from string *)
  let z2 = Marshal.from_string serialized 0 in
  let restored = Mpz.to_string z2 in
  
  printf "Original: %s\n" original;
  printf "Restored: %s\n" restored;
  printf "Match: %b\n" (original = restored);
  print_endline ""

let test_mpq () =
  print_endline "Testing Mpq serialization...";
  let q = Mpq.init () in
  Mpq.set_str q "123456789/987654321" ~base:10;
  let original = Mpq.to_string q in
  
  (* Serialize to string *)
  let serialized = Marshal.to_string q [] in
  
  (* Deserialize from string *)
  let q2 = Marshal.from_string serialized 0 in
  let restored = Mpq.to_string q2 in
  
  printf "Original: %s\n" original;
  printf "Restored: %s\n" restored;
  printf "Match: %b\n" (original = restored);
  print_endline ""

let test_mpf () =
  print_endline "Testing Mpf serialization...";
  let f = Mpf.init () in
  Mpf.set_str f "3.14159265358979323846" ~base:10;
  let original = Mpf.to_string f in
  
  (* Serialize to string *)
  let serialized = Marshal.to_string f [] in
  
  (* Deserialize from string *)
  let f2 = Marshal.from_string serialized 0 in
  let restored = Mpf.to_string f2 in
  
  printf "Original: %s\n" original;
  printf "Restored: %s\n" restored;
  printf "Match: %b\n" (original = restored);
  print_endline ""

let test_mpfr_aux f =
  print_endline "Testing Mpfr serialization...";
  let original = Mpfr.to_string f in
  
  (* Serialize to string *)
  let serialized = Marshal.to_string f [] in
  
  (* Deserialize from string *)
  let f2 = Marshal.from_string serialized 0 in
  let restored = Mpfr.to_string f2 in
  
  printf "Original: %s\n" original;
  printf "Restored: %s\n" restored;
  printf "Match: %b\n" (original = restored);
  print_endline ""

let test_mpfr () =
  let f1 = Mpfr.init () in
  Mpfr.set_str f1 "2.71828182845904523536" ~base:10 Mpfr.Near;
  test_mpfr_aux f1;

  let f2 = Mpfr.init2 200 in
  Mpfr.set_str f2 "2718281828459045235360000000000000000000000000000000000000" ~base:10 Mpfr.Near;
  test_mpfr_aux f2


let () =
  print_endline "Performing registration...";
  internal_init_custom_ops ();

  print_endline "Starting serialization tests...";
  print_endline "";
  
  test_mpz ();
  test_mpq ();
  test_mpf ();
  test_mpfr ();
  
  print_endline "All tests completed!"
