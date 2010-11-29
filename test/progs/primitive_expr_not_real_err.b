rel test {
    r: real,
}

t: test;

fn s_not(): test
{
    return t select(!r);
}
