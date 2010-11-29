rel test {
    r: real,
    s: string,
}

t: test;

rel test_res {
    r: real,
    s: string,
    x: real
}

fn s_div_err(): test_res
{
    return t extend(x = s / r);
}
