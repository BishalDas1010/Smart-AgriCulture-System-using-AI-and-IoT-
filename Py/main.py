from fastapi import FastAPI, Request, Form, File, UploadFile
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LinearRegression
import io
import json
from typing import List
from pydantic import BaseModel
import uvicorn
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="Soil Water Prediction API")

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global variable to store the trained model
model = None
feature_names = None

class PredictionInput(BaseModel):
    features: List[float]

class PredictionResult(BaseModel):
    prediction: float

@app.get("/", response_class=HTMLResponse)
async def root():
    return """
    <html>
        <head>
            <title>Soil Water Prediction API</title>
        </head>
        <body>
            <h1>Soil Water Prediction API</h1>
            <p>API is running. Visit <a href="/docs">/docs</a> for API documentation.</p>
        </body>
    </html>
    """

@app.post("/train")
async def train_model(file: UploadFile = File(...)):
    global model, feature_names
    
    # Read the uploaded CSV file
    content = await file.read()
    df = pd.read_csv(io.StringIO(content.decode('utf-8')))
    
    # Data preprocessing
    df.fillna(value=0, inplace=True)
    df.dropna(inplace=True)
    df.drop_duplicates(inplace=True)
    
    # Feature extraction
    X = df.iloc[:, :-1]
    y = df.iloc[:, -1]
    feature_names = X.columns.tolist()
    
    # Model training
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=2)
    model = LinearRegression()
    model.fit(X_train, y_train)
    
    # Evaluate model
    y_pred = model.predict(X_test)
    mse = np.mean((y_pred - y_test) ** 2)
    r2 = model.score(X_test, y_test)
    
    return {
        "message": "Model trained successfully",
        "feature_names": feature_names,
        "metrics": {
            "mse": float(mse),
            "r2": float(r2)
        }
    }

@app.post("/predict", response_model=PredictionResult)
async def predict(input_data: PredictionInput):
    global model
    
    if model is None:
        return {"error": "Model not trained yet. Please upload data and train the model first."}
    
    features = np.array(input_data.features).reshape(1, -1)
    prediction = model.predict(features)[0]
    
    return {"prediction": float(prediction)}

@app.get("/model-info")
async def get_model_info():
    global model, feature_names
    
    if model is None:
        return {"error": "Model not trained yet"}
    
    return {
        "status": "Model is trained",
        "feature_names": feature_names,
        "coefficients": model.coef_.tolist() if model else None,
        "intercept": float(model.intercept_) if model else None
    }

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)