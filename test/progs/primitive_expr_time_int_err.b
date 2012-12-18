fn test(t {i int}) {i int}
{
    return select (i > Sys.Time.Now) t;
}
