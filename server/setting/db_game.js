/*

用法1：直接在指令行执行某个函数
// mongosh --file db_game.js init myDB

用法2：直接mongosh加载这个文件

// 1. 正常连接并进入 mongosh
$ mongosh "mongodb://localhost:27017"

// 2. 加载你的工具脚本
test> load("db_game.js")
true

// 3. 此时整个终端不会被关闭！你可以随时随地手动反复调用它
test> init("myDB")
🚀 正在初始化数据库: myDB

test> backup("anotherDB")
📦 正在备份数据库: anotherDB
*/

function init(dbName) {
    print(`正在初始化数据库: ${dbName}`);
    use(dbName);
    // https://www.mongodb.com/zh-cn/docs/manual/reference/method/db.collection.createIndex/#mongodb-method-db.collection.createIndex

    // mongodb的索引是最左前缀原则，下面的索引单独查询sid有索引，查询name就没有
    // {sid: 1, key: 1, name: 1}

    const indexDefs = [
        {coll: 'global_data', keys: {sid: 1, key: 1, name: 1}, opts: {unique: true}},
        {coll: 'player_data', keys: {pid: 1, name: 1}, opts: {unique: true}},
        {coll: 'player_item', keys: {pid: 1, bid: 1}, opts: {unique: true}},
    ];

    indexDefs.forEach(function(def) {
        try {
            db[def.coll].createIndex(def.keys, def.opts);
            print(`创建索引: ${def.coll} [${Object.keys(def.keys).join(', ')}]`);
        } catch (e) {
            print(`创建索引失败: ${def.coll} -> ${e}`);
        }
    });
}

function reset(dbName) {
    print(`正在重置数据库: ${dbName}`);
    use(dbName);
    db.dropDatabase();
    init(dbName);
}

(function() {
    const args = process.argv; // 假设这是你的输入数组
    // 检查是否包含 --file
    const fileIndex = args.indexOf('--file');

    if (fileIndex !== -1) {
        // 确保 --file 后至少有两个参数（脚本路径 + 函数名）
        if (fileIndex + 2 >= args.length) {
            print('ERROR: Missing script path or function name after --file');
            quit(1);
        }

        // 从 --file 后第二个参数开始（跳过脚本文件名）
        const userArgs = args.slice(fileIndex + 2);

        const functionName = userArgs[0];
        const functionArgs = userArgs.slice(1);

        if (functionName) {
            // 从全局上下文中动态获取函数
            const targetFunction = globalThis[functionName];

            if (typeof targetFunction === 'function') {
                targetFunction.apply(null, functionArgs);
            } else {
                print(`ERROR no function [${functionName}] found!`);
                quit(1);
            }
        }
        quit(0); // 命令行模式下，执行完安全退出
    }

    // 场景 B：如果不是命令行模式（即通过 load("utils.js") 加载），这里什么都不做
    // 脚本静默加载完毕，所有函数都注入到了你的当前 Shell 窗口中，你可以像往常一样自由手动调用。
})();


