rel some_type {
    hello: int,
    world: real,
    str: string,
}

some_var: some_type;

fn some_svc(): some_type
{
    return some_var;
}

inline_test: rel {
    time, money: long,
    bid, ask: real
};

fn test(p: rel {x, y: real}): rel { x, y, z: real }
{
    return p extend(z = 0.0);
}
