rel T {
    s: string
}

fn test(t: T): T
{
    return t select(5L > long(s));
}
