rel Emp {
    salary: int,
}

rel Dep {
    name: string
}

d: Dep;

fn addUp(e: Emp): rel {avg_salary: real}
{
    return (e, d) summarize(avg_salary = avg(salary, (1.0 * salary)));
}
