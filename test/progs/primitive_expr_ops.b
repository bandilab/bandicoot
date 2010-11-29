rel test {
    i: int,
    r: real,
    s: string,
}

t: test;

rel test_res {
    i: int,
    r: real,
    s: string,
    ix: int,
    rx: real,
}

fn s_not_int(): test
{
    return t select(!i);
}

fn s_eq_int(): test
{
    return t select(i == 1);
}

fn s_eq_real(): test
{
    return t select(r == .3);
}

fn s_eq_string(): test
{
    return t select(s == "abc");
}

fn s_neq_int(): test
{
    return t select(i != 1);
}

fn s_neq_real(): test
{
    return t select(r != .3);
}

fn s_neq_string(): test
{
    return t select(s != "abc");
}

fn s_and_basic(): test
{
    return t select(i && 1);
}

fn s_and_complex(): test
{
    return t select(i == 1 && r != .3 && s == "abc");
}

fn s_or_basic(): test
{
    return t select(i || 0);
}

fn s_or_complex(): test
{
    return t select(i == 1 || r != .3 || s == "abc");
}

fn s_gt(): test
{
    return t select(i > 0 && r > 0.0 && s > "");
}

fn s_lt(): test
{
    return t select(i < 0 && r < 0.0 && s < "abc");
}

fn s_sum(): test_res
{
    return t extend(ix = i + 1, rx = r + 0.3);
}

fn s_sub(): test_res
{
    return t extend(ix = 1 - i, rx = 0.3 - r);
}

fn s_mul(): test_res
{
    return t extend(ix = 2 * i, rx = 0.1 * r);
}

fn s_div(): test_res
{
    return t extend(ix = 100 / i, rx = 100.0 / r);
}
