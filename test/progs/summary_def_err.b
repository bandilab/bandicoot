rel Emp {
    salary: int,
}

fn addUp(e: Emp): Emp
{
    return e summary(salary = add(salary, 0.0));
}
