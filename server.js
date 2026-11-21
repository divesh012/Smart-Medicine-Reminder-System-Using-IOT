
const express = require("express");
const cors = require("cors");
require("dotenv").config();

const { db, initializeDatabase } = require("./database");

const app = express();

const PORT = process.env.PORT || 5000;
const PASS_KEY = process.env.PASS_KEY || "123456";

app.use(cors());
app.use(express.json());

// -------------------------------------
// Helper: Format medicine data
// -------------------------------------

const getMedicines = () => {

    const rows = db.prepare(`
        SELECT
            id,
            name,
            enabled,
            days,
            reminder_time
        FROM medicines
        ORDER BY id
    `).all();

    return rows.map((medicine) => ({
        id: medicine.id,
        name: medicine.name,
        enabled: Boolean(medicine.enabled),
        days: JSON.parse(medicine.days || "[]"),
        time: medicine.reminder_time || ""
    }));

};


// -------------------------------------
// Home Route
// -------------------------------------

app.get("/", (req, res) => {

    res.json({
        success: true,
        message: "Smart Medicine Reminder Backend Running"
    });

});


// -------------------------------------
// Passkey Verification
// -------------------------------------

app.post("/api/verify-passkey", (req, res) => {

    const { passKey } = req.body;

    if (!passKey) {
        return res.status(400).json({
            success: false,
            message: "Pass key is required"
        });
    }

    if (passKey === PASS_KEY) {
        return res.json({
            success: true,
            message: "Passkey verified successfully"
        });
    }

    return res.status(401).json({
        success: false,
        message: "Invalid pass key"
    });

});


// -------------------------------------
// Get All Medicines
// -------------------------------------

app.get("/api/medicines", (req, res) => {

    try {

        const medicines = getMedicines();

        res.json(medicines);

    } catch (error) {

        console.error(
            "Error getting medicines:",
            error.message
        );

        res.status(500).json({
            success: false,
            message: "Failed to load medicines"
        });

    }

});


// -------------------------------------
// Update Medicine Schedule
// -------------------------------------

app.post("/api/medicines", (req, res) => {

    try {

        const { medicines } = req.body;

        console.log("-----------------------------------");
        console.log("Received medicines from frontend:");
        console.log(JSON.stringify(medicines, null, 2));
        console.log("-----------------------------------");


        // Validate medicine slots

        if (
            !Array.isArray(medicines) ||
            medicines.length !== 4
        ) {

            return res.status(400).json({
                success: false,
                message: "Exactly 4 medicine slots are required"
            });

        }


        // Validate each medicine

        const ids = new Set();

        for (const medicine of medicines) {

            if (
                !Number.isInteger(Number(medicine.id)) ||
                ![1, 2, 3, 4].includes(Number(medicine.id)) ||
                ids.has(Number(medicine.id))
            ) {

                return res.status(400).json({
                    success: false,
                    message: "Invalid or duplicate medicine slot ID"
                });

            }

            ids.add(Number(medicine.id));

            if (medicine.enabled) {

                if (
                    !Array.isArray(medicine.days) ||
                    medicine.days.length === 0
                ) {

                    return res.status(400).json({
                        success: false,
                        message:
                            `${medicine.name}: Please select at least one day`
                    });

                }

                if (!medicine.time) {

                    return res.status(400).json({
                        success: false,
                        message:
                            `${medicine.name}: Please select reminder time`
                    });

                }

            }

        }


        // SQLite transaction

        const updateMedicine = db.prepare(`
            UPDATE medicines
            SET
                name = ?,
                enabled = ?,
                days = ?,
                reminder_time = ?
            WHERE id = ?
        `);


        const saveMedicines = db.transaction((medicineList) => {

            for (const medicine of medicineList) {

                const enabled = Boolean(medicine.enabled);

                const days = enabled
                    ? medicine.days
                    : [];

                const time = enabled
                    ? medicine.time
                    : null;

                console.log(
                    `Updating Medicine ${medicine.id}:`,
                    {
                        name: medicine.name,
                        enabled,
                        days,
                        time
                    }
                );


                const result = updateMedicine.run(
                    medicine.name || `Medicine ${medicine.id}`,
                    enabled ? 1 : 0,
                    JSON.stringify(days),
                    time,
                    Number(medicine.id)
                );


                if (result.changes === 0) {

                    throw new Error(
                        `Medicine ID ${medicine.id} was not found in database`
                    );

                }

                console.log(
                    `Medicine ${medicine.id} updated successfully.`
                );

            }

        });


        // Save all four slots together.
        // If one update fails, all updates are rolled back.

        saveMedicines(medicines);


        // Get updated records

        const updatedMedicines = getMedicines();


        console.log("-----------------------------------");
        console.log("DATABASE AFTER UPDATE:");
        console.log(JSON.stringify(updatedMedicines, null, 2));
        console.log("-----------------------------------");


        res.json({
            success: true,
            message: "Medicine schedule saved successfully",
            medicines: updatedMedicines
        });


    } catch (error) {

        console.error(
            "Error saving medicines:",
            error.message
        );

        res.status(500).json({
            success: false,
            message: "Failed to save medicine schedule",
            error: error.message
        });

    }

});


// -------------------------------------
// ESP32 Device Status
// -------------------------------------

app.get("/api/device-status", (req, res) => {

    res.json({
        connected: false,
        message: "ESP32 not connected"
    });

});


// -------------------------------------
// Start Server After Database Setup
// -------------------------------------

const startServer = async () => {

    const initialized = await initializeDatabase();

    if (!initialized) {

        console.error("Server startup cancelled: Database initialization failed.");

        process.exit(1);

    }

    app.listen(PORT, () => {

        console.log("-----------------------------------");
        console.log("Smart Medicine Reminder Backend");
        console.log("-----------------------------------");
        console.log(`Server running on port ${PORT}`);
        console.log("Database: SQLite");
        console.log("-----------------------------------");

    });

};

startServer();
