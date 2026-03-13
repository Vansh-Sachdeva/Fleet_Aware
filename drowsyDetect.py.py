
import cv2
import mediapipe as mp
import numpy as np
import gspread
from oauth2client.service_account import ServiceAccountCredentials

# 1. GOOGLE SHEET CONNECTION
scope = [
"https://spreadsheets.google.com/feeds",
"https://www.googleapis.com/auth/drive"
]

creds = ServiceAccountCredentials.from_json_keyfile_name(r"C:\Users\bit\Desktop\kam k baat\daru badnam\credentials.json.json", scope)
client = gspread.authorize(creds)

try:
    status_sheet = client.open("DriverData").worksheet("Status")
    print("Successfully connected to Google Sheets!")
except Exception as e:
    print("Error connecting to Google Sheets. Make sure you have a tab named 'Status'.")
    exit()

# 2. MEDIAPIPE SETUP
mp_face_mesh = mp.solutions.face_mesh
face_mesh = mp_face_mesh.FaceMesh(
    static_image_mode=False,
    max_num_faces=1,
    refine_landmarks=True
)

LEFT_EYE = [33, 160, 158, 133, 153, 144]
RIGHT_EYE = [362, 385, 387, 263, 373, 380]

def calculate_EAR(eye_points, landmarks, w, h):
    points = []
    for idx in eye_points:
        x = int(landmarks[idx].x * w)
        y = int(landmarks[idx].y * h)
        points.append((x, y))

    A = np.linalg.norm(np.array(points[1]) - np.array(points[5]))
    B = np.linalg.norm(np.array(points[2]) - np.array(points[4]))
    C = np.linalg.norm(np.array(points[0]) - np.array(points[3]))

    ear = (A + B) / (2.0 * C)
    return ear


# 3. CAMERA AND LOGIC SETUP
cap = cv2.VideoCapture(0)

EAR_THRESHOLD = 0.20
FRAME_THRESHOLD = 20

counter = 0
drowsy_status = "AWAKE"
previous_status = "UNKNOWN"

print("System running... Press 'q' to quit.")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    h, w = frame.shape[:2]

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = face_mesh.process(rgb)

    if results.multi_face_landmarks:
        for face_landmarks in results.multi_face_landmarks:

            landmarks = face_landmarks.landmark

            left_ear = calculate_EAR(LEFT_EYE, landmarks, w, h)
            right_ear = calculate_EAR(RIGHT_EYE, landmarks, w, h)

            ear = (left_ear + right_ear) / 2.0

            if ear < EAR_THRESHOLD:
                counter += 1

                if counter >= FRAME_THRESHOLD:
                    drowsy_status = "DROWSY"
                    cv2.putText(frame, "DROWSINESS ALERT!",
                                (50,100),
                                cv2.FONT_HERSHEY_SIMPLEX,
                                1.2,
                                (0,0,255),
                                3)
            else:
                counter = 0
                drowsy_status = "AWAKE"

    # UPDATE GOOGLE SHEET ONLY IF STATUS CHANGES
    if drowsy_status != previous_status:
        try:
            status_sheet.update_acell('A1', drowsy_status)
            previous_status = drowsy_status
        except:
            print("Sheet update error")

    cv2.imshow("Drowsiness Detection", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()

