rel test {
    r: real,
    s: string,
}

t: test;

rel test_res {
    r: real,
    s: string,
    x: string,
}

fn s_sub_err(): test_res
{
    return t extend(x = s - r);
}
