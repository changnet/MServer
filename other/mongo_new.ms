use test_999
db.createUser( {user:"test",pwd:"test",roles:["dbOwner","userAdmin","readWrite"]} )

db.auth( "test","test" )
db.uniqueid.createIndex( {_id:1},{ unique:true } )
db.account.createIndex( {_id:1},{ unique:true } )