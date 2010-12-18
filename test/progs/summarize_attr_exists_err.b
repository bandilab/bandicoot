rel Emp {
    name: string,
    salary: int,
}

rel Dep {
    name: string,
    salary: int
}

d: Dep;

fn addUp(e: Emp): rel {salary: real}
{
    return (e, d) summarize(salary = avg(salary, (1.0 * salary)));
}
