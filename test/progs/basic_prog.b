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
