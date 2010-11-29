rel Emp {
    name: string,
}

fn addUp(e: Emp): Emp
{
    return e summarize(name = min(name, ""));
}
