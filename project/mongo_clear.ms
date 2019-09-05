var collections = db.getCollectionNames()

var sys = "system."
for ( var i = 0;i < collections.length;i ++ ) {
    var name = collections[i]
    if ( name.substring(0,sys.length) != sys ) {
        db[name].drop()
        print( "drop collection",name )
    }
}