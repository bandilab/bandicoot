type T {l long}

fn test(t {i int}) T
{
    var x = select ((long i) == Sys.Time.Now + 10L) t;
    return extend (l = Sys.Time.Now) t;
}
