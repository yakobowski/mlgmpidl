(*
 * Test program for serialization support in mlgmpidl.
 * All tests use assertions — any failure exits with a non-zero status.
 *)

let failures = ref 0
let tests = ref 0

let check name ok =
  incr tests;
  if not ok then begin
    Printf.eprintf "FAIL: %s\n%!" name;
    incr failures
  end

(* ------------------------------------------------------------------ *)
(* Mpz tests                                                          *)
(* ------------------------------------------------------------------ *)

let roundtrip_mpz z =
  let s = Marshal.to_string z [] in
  (Marshal.from_string s 0 : Mpz.t)

let test_mpz_value name str =
  let z = Mpz.init () in
  Mpz.set_str z str ~base:10;
  let z2 = roundtrip_mpz z in
  check (name ^ " string") (Mpz.to_string z = Mpz.to_string z2);
  check (name ^ " cmp")    (Mpz.cmp z z2 = 0)

let test_mpz () =
  (* Zero *)
  test_mpz_value "mpz zero" "0";

  (* Small positive *)
  test_mpz_value "mpz small pos" "42";

  (* Small negative *)
  test_mpz_value "mpz small neg" "-42";

  (* One *)
  test_mpz_value "mpz one" "1";
  test_mpz_value "mpz minus one" "-1";

  (* Large positive *)
  test_mpz_value "mpz large pos" "123456789012345678901234567890";

  (* Large negative *)
  test_mpz_value "mpz large neg" "-123456789012345678901234567890";

  (* Power of 2 *)
  let z = Mpz.init () in
  Mpz.ui_pow_ui z 2 256;
  let z2 = roundtrip_mpz z in
  check "mpz 2^256 cmp" (Mpz.cmp z z2 = 0);

  (* Very large: 2^1024 - 1 *)
  let big = Mpz.init () in
  Mpz.ui_pow_ui big 2 1024;
  let one = Mpz.init_set_si 1 in
  Mpz.sub big big one;
  let big2 = roundtrip_mpz big in
  check "mpz 2^1024-1 cmp" (Mpz.cmp big big2 = 0);

  (* Marshal to file and back *)
  let z = Mpz.init () in
  Mpz.set_str z "999999999999999999999" ~base:10;
  let fname = Filename.temp_file "mpz_test" ".marshal" in
  let oc = open_out_bin fname in
  Marshal.to_channel oc z [];
  close_out oc;
  let ic = open_in_bin fname in
  let z2 : Mpz.t = Marshal.from_channel ic in
  close_in ic;
  Sys.remove fname;
  check "mpz file roundtrip" (Mpz.cmp z z2 = 0)

(* ------------------------------------------------------------------ *)
(* Mpq tests                                                          *)
(* ------------------------------------------------------------------ *)

let roundtrip_mpq q =
  let s = Marshal.to_string q [] in
  (Marshal.from_string s 0 : Mpq.t)

let test_mpq_value name str =
  let q = Mpq.init () in
  Mpq.set_str q str ~base:10;
  let q2 = roundtrip_mpq q in
  check (name ^ " string") (Mpq.to_string q = Mpq.to_string q2);
  check (name ^ " cmp")    (Mpq.cmp q q2 = 0)

let test_mpq () =
  test_mpq_value "mpq zero" "0";
  test_mpq_value "mpq one" "1";
  test_mpq_value "mpq minus one" "-1";
  test_mpq_value "mpq fraction" "123456789/987654321";
  test_mpq_value "mpq negative fraction" "-355/113";
  test_mpq_value "mpq large" "99999999999999999999/100000000000000000001";

  (* Non-canonical rational (numerator and denominator share a factor) *)
  let q = Mpq.init () in
  Mpq.set_str q "6/4" ~base:10;
  (* Don't canonicalize — test that raw num/den survive *)
  let q2 = roundtrip_mpq q in
  check "mpq non-canonical" (Mpq.to_string q = Mpq.to_string q2)

(* ------------------------------------------------------------------ *)
(* Mpf tests                                                          *)
(* ------------------------------------------------------------------ *)

let roundtrip_mpf f =
  let s = Marshal.to_string f [] in
  (Marshal.from_string s 0 : Mpf.t)

let test_mpf_value name str =
  let f = Mpf.init () in
  Mpf.set_str f str ~base:10;
  let f2 = roundtrip_mpf f in
  check (name ^ " cmp") (Mpf.cmp f f2 = 0);
  check (name ^ " prec") (Mpf.get_prec f = Mpf.get_prec f2)

let test_mpf () =
  test_mpf_value "mpf zero" "0";
  test_mpf_value "mpf pi" "3.14159265358979323846";
  test_mpf_value "mpf neg" "-2.71828182845904523536";
  test_mpf_value "mpf one" "1.0";
  test_mpf_value "mpf small" "0.000000001";
  test_mpf_value "mpf large" "123456789012345.6789";

  (* High precision *)
  let f = Mpf.init2 512 in
  Mpf.set_str f "3.14159265358979323846264338327950288" ~base:10;
  let f2 = roundtrip_mpf f in
  check "mpf high prec cmp" (Mpf.cmp f f2 = 0);
  check "mpf high prec prec" (Mpf.get_prec f = Mpf.get_prec f2);

  (* Negative zero: mpf doesn't really have -0, so just test 0 *)
  let f = Mpf.init () in
  Mpf.set_si f 0;
  let f2 = roundtrip_mpf f in
  check "mpf zero cmp" (Mpf.cmp f f2 = 0)

(* ------------------------------------------------------------------ *)
(* Mpfr tests                                                         *)
(* ------------------------------------------------------------------ *)

let roundtrip_mpfr f =
  let s = Marshal.to_string f [] in
  (Marshal.from_string s 0 : Mpfr.t)

let test_mpfr_finite name str ?prec () =
  let f = match prec with
    | None -> Mpfr.init ()
    | Some p -> Mpfr.init2 p
  in
  ignore (Mpfr.set_str f str ~base:10 Mpfr.Near);
  let f2 = roundtrip_mpfr f in
  check (name ^ " cmp") (Mpfr.cmp f f2 = 0);
  check (name ^ " prec") (Mpfr.get_prec f = Mpfr.get_prec f2)

let test_mpfr () =
  (* Finite values *)
  test_mpfr_finite "mpfr e" "2.71828182845904523536" ();
  test_mpfr_finite "mpfr neg large" "-2718281828459045235360000000000000000000000000000000000000" ~prec:200 ();
  test_mpfr_finite "mpfr tiny" "0.00000000000000000000000000000000000000000000000000000000000000000001" ~prec:200 ();
  test_mpfr_finite "mpfr low prec" "3.14" ~prec:20 ();
  test_mpfr_finite "mpfr one" "1.0" ();
  test_mpfr_finite "mpfr neg one" "-1.0" ();

  (* Zero *)
  let z = Mpfr.init () in
  ignore (Mpfr.set_str z "0" ~base:10 Mpfr.Near);
  let z2 = roundtrip_mpfr z in
  check "mpfr zero is zero" (Mpfr.zero_p z2);
  check "mpfr zero sign" (Mpfr.signbit z2 = 0);

  (* Negative zero *)
  let nz = Mpfr.init () in
  ignore (Mpfr.set_str nz "-0" ~base:10 Mpfr.Near);
  let nz2 = roundtrip_mpfr nz in
  check "mpfr -0 is zero" (Mpfr.zero_p nz2);
  check "mpfr -0 sign" (Mpfr.signbit nz2 <> 0);

  (* NaN *)
  let nan_val = Mpfr.init () in
  ignore (Mpfr.set_str nan_val "nan" ~base:10 Mpfr.Near);
  let nan2 = roundtrip_mpfr nan_val in
  check "mpfr nan" (Mpfr.nan_p nan2);

  (* +Inf *)
  let pinf = Mpfr.init () in
  ignore (Mpfr.set_str pinf "inf" ~base:10 Mpfr.Near);
  let pinf2 = roundtrip_mpfr pinf in
  check "mpfr +inf is inf" (Mpfr.inf_p pinf2);
  check "mpfr +inf sign" (Mpfr.sgn pinf2 > 0);

  (* -Inf *)
  let ninf = Mpfr.init () in
  ignore (Mpfr.set_str ninf "-inf" ~base:10 Mpfr.Near);
  let ninf2 = roundtrip_mpfr ninf in
  check "mpfr -inf is inf" (Mpfr.inf_p ninf2);
  check "mpfr -inf sign" (Mpfr.sgn ninf2 < 0);

  (* Precision preservation *)
  let f = Mpfr.init2 256 in
  ignore (Mpfr.set_str f "1.23456789" ~base:10 Mpfr.Near);
  let f2 = roundtrip_mpfr f in
  check "mpfr prec 256" (Mpfr.get_prec f2 = 256);

  (* Marshal to file *)
  let f = Mpfr.init () in
  ignore (Mpfr.set_str f "2.718281828" ~base:10 Mpfr.Near);
  let fname = Filename.temp_file "mpfr_test" ".marshal" in
  let oc = open_out_bin fname in
  Marshal.to_channel oc f [];
  close_out oc;
  let ic = open_in_bin fname in
  let f2 : Mpfr.t = Marshal.from_channel ic in
  close_in ic;
  Sys.remove fname;
  check "mpfr file roundtrip" (Mpfr.cmp f f2 = 0)

(* ------------------------------------------------------------------ *)
(* Main                                                               *)
(* ------------------------------------------------------------------ *)

let () =
  (* No manual registration needed — it happens automatically via module init *)
  test_mpz ();
  test_mpq ();
  test_mpf ();
  test_mpfr ();

  Gc.full_major ();

  Printf.printf "%d tests, %d failures\n%!" !tests !failures;
  if !failures > 0 then
    exit 1
  else
    Printf.printf "All tests passed.\n%!"
