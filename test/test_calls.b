type Books {
    title string,
    price real,
}

var shelf Books;

fn Return() Books {
    return shelf;
}

fn Echo(b Books) Books {
    return b;
}

fn TmpReturn(b Books) Books {
    var tmp = b + shelf;
    return tmp;
}

fn ReRead(b Books) void {
    shelf = shelf;
}

fn StoreReturn(b Books) Books {
    shelf += b;
    return shelf;
}

fn IndirectReturn() Books {
    return Return;
}

fn IndirectStoreReturn(b Books) Books {
    return StoreReturn b;
}

fn IndirectEcho(b Books) Books {
    return Echo b;
}

fn IndirectTmpReturn(b Books) Books {
    return TmpReturn b;
}

fn IndirectReRead(b Books) void {
    ReRead b;
}
