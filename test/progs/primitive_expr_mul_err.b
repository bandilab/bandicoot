rel test {
    i: int,
    s: string,
}

t: test;

rel test_res {
    i: int,
    s: string,
    x: int,
}

fn s_mul_err(): test_res
{
    return t extend(x = i * s);
}
