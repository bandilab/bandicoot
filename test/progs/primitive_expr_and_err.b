rel test {
    i: int,
    s: string,
}

t: test;

fn s_and_err(): test
{
    return t select(s && i);
}
