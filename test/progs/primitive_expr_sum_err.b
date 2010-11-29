rel test {
    i: int,
    r: real,
}

t: test;

rel test_res {
    i: int,
    r: real,
    x: int,
}

fn s_sum_err()
{
    return t extend(x = i + r);
}
