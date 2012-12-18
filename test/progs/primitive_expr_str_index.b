fn test(x {s string}) {s string}
{
    return select ((Sys.String.Index s "hello") > -1) x;
}
