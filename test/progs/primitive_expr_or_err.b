rel test {
    i: int,
    r: real,
}

t: test;

fn s_or(): test
{
    return t select(r || i);
}
