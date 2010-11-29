rel test {
    s: string,
}

t: test;

fn s_not(): test
{
    return t select(!s);
}
