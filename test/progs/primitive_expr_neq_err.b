rel test {
    i: int,
    r: real,
}

t: test;

fn s_neq_err(): test
{
    return t select(i != r);
}
