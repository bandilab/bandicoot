rel Emp {
    salary: int,
}

fn addUp(e: Emp): rel {avg_salary: real}
{
    return e summarize(avg_salary = avg(salary, (1.0 * salary)));
}
