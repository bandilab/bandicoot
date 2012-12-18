fn test(x {s string}) {s string}
{
    return select ((String.Index s "hello") > -1) x;
}
