rel Emp {
    salary: int,
}

fn addUp(e: Emp): Emp
{
    return e summarize(salary = add(salary, 0.0));
}
