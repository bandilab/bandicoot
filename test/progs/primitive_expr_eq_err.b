rel test {
    r: real,
    s: string,
}

t: test;

fn s_eq_err(): test
{
    return t select(r == s);
}
