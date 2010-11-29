rel test {
    i: int,
    l: long,
    r: real,
    s: string,
}

t: test;

rel test_res {
    i: int,
    l: long,
    r: real,
    s: string,
    i_calc: int,
    l_calc: long,
    r_calc: real,
    b_icalc: int,
    b_lcalc: int,
    b_rcalc: int,
    b_scalc: int,
    b_xcalc1: int,
    b_xcalc2: int,
}

fn s_primitive_expr(): test_res
{
    return t extend(i_calc = ((i + 3) / 10) - (i * 2),
                    l_calc = ((l + 3L) / 10L) - (l * 2L),
                    r_calc = ((r + 3.0) / 10.0) - (r * 2.0),
                    b_icalc = !((i == 2) && (2 < i || i > 5)),
                    b_lcalc = !((l == 2L) && (2L < l || l > 5L)),
                    b_rcalc = (r == 0.3) && (r < 0.1 || r > 0.7),
                    b_scalc = (s == "abc") && (s < "zzz"),
                    b_xcalc1 = !("s" != s || r < (r + 0.12)),
                    b_xcalc2 = i < 7 || r > 0.53 || s == "x" && l == 1L);
}
