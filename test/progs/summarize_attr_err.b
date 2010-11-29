rel Emp {
    salary: int,
}

fn addUp(e: Emp): rel {salary: real}
{
    return e summarize(salary = avg(s, 0.0));
}
