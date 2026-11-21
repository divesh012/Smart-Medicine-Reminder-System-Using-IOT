
const Database = require("better-sqlite3");
const path = require("path");

const dbPath = path.join(__dirname, "medicine.db");

const db = new Database(dbPath);

// Enable foreign key support
db.pragma("foreign_keys = ON");

// Database initialization
const initializeDatabase = async () => {
    try {

        console.log("-----------------------------------");
        console.log("SQLite database connected.");
        console.log("Database:", dbPath);
        console.log("-----------------------------------");

        // Create medicines table
        db.exec(`
            CREATE TABLE IF NOT EXISTS medicines (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                enabled INTEGER NOT NULL DEFAULT 0,
                days TEXT NOT NULL DEFAULT '[]',
                reminder_time TEXT
            );
        `);

        console.log("Medicines table ready.");

        // Default medicine slots
        const defaultMedicines = [
            {
                id: 1,
                name: "Medicine 1",
                enabled: 1,
                days: JSON.stringify([
                    "Monday", "Tuesday", "Wednesday",
                    "Thursday", "Friday", "Saturday", "Sunday"
                ]),
                time: "08:00"
            },
            {
                id: 2,
                name: "Medicine 2",
                enabled: 1,
                days: JSON.stringify([
                    "Monday", "Tuesday", "Wednesday",
                    "Thursday", "Friday", "Saturday", "Sunday"
                ]),
                time: "13:00"
            },
            {
                id: 3,
                name: "Medicine 3",
                enabled: 0,
                days: JSON.stringify([]),
                time: null
            },
            {
                id: 4,
                name: "Medicine 4",
                enabled: 0,
                days: JSON.stringify([]),
                time: null
            }
        ];

        const insertMedicine = db.prepare(`
            INSERT OR IGNORE INTO medicines
            (id, name, enabled, days, reminder_time)
            VALUES (?, ?, ?, ?, ?)
        `);

        const insertMany = db.transaction((medicines) => {
            for (const medicine of medicines) {
                insertMedicine.run(
                    medicine.id,
                    medicine.name,
                    medicine.enabled,
                    medicine.days,
                    medicine.time
                );
            }
        });

        insertMany(defaultMedicines);

        console.log("Default medicine slots ready.");

        return true;

    } catch (error) {
        console.error(
            "Database initialization failed:",
            error.message
        );

        return false;
    }
};

module.exports = {
    db,
    initializeDatabase
};
