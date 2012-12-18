fn test(x {s string}) {s string}
{
    return select ((Sys.String.Index s 0) > -1) x;
}
