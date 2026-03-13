function doGet(e) {

var ss = SpreadsheetApp.getActiveSpreadsheet();
var dataSheet = ss.getSheetByName("Data");
var statusSheet = ss.getSheetByName("Status");

if (!e.parameter.type) {
return ContentService.createTextOutput("Invalid request");
}

var type = e.parameter.type;

if (type == "write") {

dataSheet.appendRow([
new Date(),
e.parameter.alcohol || 0,
e.parameter.gas || 0,
e.parameter.ax || 0,
e.parameter.ay || 0,
e.parameter.az || 0,
e.parameter.rash || "Normal",
e.parameter.lat || 0,
e.parameter.lng || 0
]);

return ContentService.createTextOutput("Logged");
}

if (type == "read") {

var status = statusSheet.getRange("A1").getValue();

return ContentService.createTextOutput(status);
}

if (type == "web_data") {

var lastRow = dataSheet.getLastRow();
var row = dataSheet.getRange(lastRow,1,1,9).getValues()[0];

var status = statusSheet.getRange("A1").getValue();

var data = {
time: row[0],
alcohol: row[1],
gas: row[2],
ax: row[3],
ay: row[4],
az: row[5],
rash: row[6],
lat: row[7],
lng: row[8],
drowsiness: status
};

return ContentService
.createTextOutput(JSON.stringify(data))
.setMimeType(ContentService.MimeType.JSON);
}

return ContentService.createTextOutput("Unknown request");
}