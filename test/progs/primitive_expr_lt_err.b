rel test {
    i: int,
    r: real,
}

t: test;

fn s_lt_err(): test
{
    return t select(r < i);
}
