//
//  main.cpp
//  LearnOpenGL
//
//  Created by changbo on 2019/10/12.
//  Copyright © 2019 CB. All rights reserved.
//

#include "GLTools.h"
#include "GLShaderManager.h"
#include "GLFrustum.h"
#include "GLBatch.h"
#include "GLMatrixStack.h"
#include "GLGeometryTransform.h"
#include "StopWatch.h"

#include <math.h>
#include <stdio.h>

#ifdef __APPLE__
#include <glut/glut.h>
#else
#define FREEGLUT_STATIC
#include <GL/glut.h>
#endif


// 添加随机小球
#define NUM_SPHERES 50
GLFrame spheres[NUM_SPHERES];

// 着色器管理器
GLShaderManager        shaderManager;

// 模型视图矩阵
GLMatrixStack          modelViewMatrix;

// 投影矩阵
GLMatrixStack          projectionMatrix;

// 视景体
GLFrustum              viewFrustum;

// 几何图形变换管道
GLGeometryTransform    transformPipeline;

// 花托批处理
GLTriangleBatch        torusBatch;

// 地板批处理
GLBatch                floorBatch;

//** 定义公转球的批处理（公转自转）**
//球批处理
GLTriangleBatch        sphereBatch;

//** 角色帧 照相机角色帧（全局照相机实例）
GLFrame                cameraFrame;



// 为程序作一次性的设置
void SetupRC()
{
    // 初始化着色器管理器
    shaderManager.InitializeStockShaders();
    
    // 开启深度测试
    glEnable(GL_DEPTH_TEST);
    
    // 设置清屏颜色到颜色缓存区
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // 绘制游泳圈
    gltMakeTorus(torusBatch, 0.4f, 0.15f, 30, 30);
    
    // 球体（公转自转）
    gltMakeSphere(sphereBatch, 0.1f, 26, 13);
    
    // 往地板批处理中添加顶点数据
    floorBatch.Begin(GL_LINES, 324);
    for (GLfloat x = -20.0; x<=20.0f; x+=0.5) {
        floorBatch.Vertex3f(x, -0.55f, 20.0f);
        floorBatch.Vertex3f(x, -0.55f, -20.0f);
        
        floorBatch.Vertex3f(20.0f, -0.55f, x);
        floorBatch.Vertex3f(-20.0f, -0.55, x);
    }
    floorBatch.End();
    
    // 在场景中随机位置，初始化许多的小球体
    for (int i=0; i<NUM_SPHERES; i++) {
        //y轴不变，x，z随机值
        GLfloat x = ((GLfloat)((rand()%400)-200)*0.1f);
        GLfloat z = ((GLfloat)((rand()%400)-200)*0.1f);
        
        // 设置球体y方向为0.0，可以看起来漂浮在眼睛的高度
        // 对spheres数组中每一个顶点，设置顶点数据
        spheres[i].SetOrigin(x, 0.0f, z);
        
    }
}


//窗口大小改变时接受新的宽度和高度，其中0,0代表窗口中视口的左下角坐标，w，h代表像素
void ChangeSize(int w,int h)
{
    glViewport(0,0, w, h);
    
    // 创建投影矩阵
    viewFrustum.SetPerspective(45.0f, float(w)/float(h), 1.0f, 100.0f);
    
    // 加载到投影矩阵堆栈上
    projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());
    
    // 设置变换管道以使用两个矩阵堆栈（变换矩阵modelViewMatrix ，投影矩阵projectionMatrix）
    // 初始化GLGeometryTransform 的实例transformPipeline.通过将它的内部指针设置为模型视图矩阵堆栈 和 投影矩阵堆栈实例，来完成初始化
    // 当然这个操作也可以在SetupRC 函数中完成，但是在窗口大小改变时或者窗口创建时设置它们并没有坏处。而且这样可以一次性完成矩阵和管线的设置。
    transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
}


//开始渲染 绘制场景
void RenderScene(void)
{
    // 地板颜色值
    static GLfloat vFloorColor[] = {0.52f, 0.81f, 0.98f, 1.0f};
    // 游泳圈颜色值
    static GLfloat vTorusColor[] = {0.94f, 0.94f, 0.50f, 1.0f};
    // 公转自转球颜色值
    static GLfloat vSphereColor[] = {1.0f, 0.89f, 0.77f, 1.0f};
    // 小球颜色值
    static GLfloat smSphereColor[] = {0.87f, 0.72f, 0.53f, 1.0f};
    // 基于时间动画
    static CStopWatch rotTimer;
    float yRot = rotTimer.GetElapsedSeconds()*60.0f;
    
    // 清除颜色缓存区，深度缓存区，模版缓存区
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    // 因为我们先绘制地面，而地面是不需要有任何变换的。所以在开始渲染时保证矩阵状态，
    // 然后在结束时使用相应的PopMatrix恢复它。这样就不必在每一次渲染时重载单位矩阵了。
    
    // 将当前模型视图矩阵压入矩阵堆栈（单位矩阵）
    modelViewMatrix.PushMatrix();
    
    
    // 设置照相机矩阵
    M3DMatrix44f mCamera;
    // 从camraFrame中获取一个4*4的矩阵
    cameraFrame.GetCameraMatrix(mCamera);
    // 将照相机矩阵压入模型视图堆栈中
    modelViewMatrix.PushMatrix(mCamera);
    
    // 光源位置的全局坐标存储在vLightPos变量中，其中包含了光源位置x、y、z坐标和w坐标
    // 我们必须保留w坐标为1.0。因为无法用一个3分量去乘以4*4矩阵
    
    
    // 添加光源
    M3DVector4f vLightPos = {0.0f, 10.0f, 5.0f, 1.0f};
    M3DVector4f vLightEyePos;
    // 将照相机矩阵 mCamera 与 光源矩阵 vLightPos 相乘获得 vLightEyePos 矩阵
    m3dTransformVector4(vLightEyePos, vLightPos, mCamera);
    
    // 绘制地面（平面着色器）
    shaderManager.UseStockShader(GLT_SHADER_FLAT,
                                 transformPipeline.GetModelViewProjectionMatrix(),
                                 vFloorColor);
    
    floorBatch.Draw();
    
    
    // 绘制悬浮球体 (使用 sphereBatch 绘制)
    //思路：循环绘制50个悬浮球体，绘制一个压栈一个,绘制完成一个出栈一个
    for (int i = 0; i < NUM_SPHERES; i++) {
        modelViewMatrix.PushMatrix();
        modelViewMatrix.MultMatrix(spheres[i]);
        
        // 绘制光源，修改着色器管理器
        // 参数1: GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF
        // 参数2: 模型视图矩阵
        // 参数3: 投影矩阵
        // 参数4: 视点坐标系中的光源位置
        // 参数5: 基本漫反射颜色
        // 参数6: 颜色
        shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,
                                     transformPipeline.GetModelViewMatrix(),
                                     transformPipeline.GetProjectionMatrix(),
                                     vLightEyePos, smSphereColor);
        
        sphereBatch.Draw();
        
        modelViewMatrix.PopMatrix();
    }
    
    
    // 绘制游泳圈
    // modelViewMatrix 顶部矩阵沿着z轴移动2.5单位
    modelViewMatrix.Translate(0.0, 0.0, -2.5f);
    
    // 保存平移（公转自转）
    modelViewMatrix.PushMatrix();
    
    // modelViewMatrix 顶部矩阵旋转yRot度
    modelViewMatrix.Rotate(yRot, 0.0f, 1.0f, 0.0f);
    
    
    // 使用平面着色器 变换管道中的投影矩阵 和 变换矩阵 相乘的矩阵，指定游泳圈颜色
    // 绘制光源，修改着色器管理器
    shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,
                                 transformPipeline.GetModelViewMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightEyePos, vTorusColor);
    
    torusBatch.Draw();
    
    
    // 恢复modelViewMatrix矩阵，移除矩阵堆栈
    // 使用PopMatrix推出刚刚变换的矩阵，然后恢复到单位矩阵
    modelViewMatrix.PopMatrix();
    
    
    // 绘制公转球体（公转自转）
    modelViewMatrix.Rotate(yRot * -2.0f, 0.0f, 1.0f, 0.0f);
    modelViewMatrix.Translate(0.8f, 0.0f, 0.0f);
    
    // 绘制光源，修改着色器管理器
    shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,
                                 transformPipeline.GetModelViewMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightEyePos, vSphereColor);
    
    sphereBatch.Draw();
    
    
    // 恢复矩阵（公转自转）
    modelViewMatrix.PopMatrix();
    // 恢复矩阵
    modelViewMatrix.PopMatrix();
    
    
    // 执行缓存区交换
    glutSwapBuffers();
    
    // 告诉glut在显示一遍
    glutPostRedisplay();
}



// 移动照相机参考帧，来对方向键作出响应
void SpeacialKeys(int key,int x,int y)
{

    float linear = 0.2f;
    float angular = float(m3dDegToRad(4.0f));
    
    if (key == GLUT_KEY_UP) {
       
        //MoveForward 平移
        cameraFrame.MoveForward(linear);
    }
    
    if (key == GLUT_KEY_DOWN) {
        cameraFrame.MoveForward(-linear);
    }
    
    if (key == GLUT_KEY_LEFT) {
        //RotateWorld 旋转
        cameraFrame.RotateWorld(angular, 0.0f, 1.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_RIGHT) {
        cameraFrame.RotateWorld(-angular, 0.0f, 1.0f, 0.0f);
    }
}


int main(int argc,char* argv[])
{
    //设置当前工作目录，针对MAC OS X
    gltSetWorkingDirectory(argv[0]);
    
    //初始化GLUT库
    glutInit(&argc, argv);
    
    /*初始化双缓冲窗口，其中标志GLUT_DOUBLE、GLUT_RGBA、GLUT_DEPTH、GLUT_STENCIL分别指
     双缓冲窗口、RGBA颜色模式、深度测试、模板缓冲区*/
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_STENCIL);
    
    //GLUT窗口大小，标题窗口
    glutInitWindowSize(800,600);
    
    glutCreateWindow("OpenGL SphereWorld");
    
    //注册回调函数
    glutReshapeFunc(ChangeSize);
    
    glutDisplayFunc(RenderScene);
    
    glutSpecialFunc(SpeacialKeys);
    
    //驱动程序的初始化中没有出现任何问题。
    GLenum err = glewInit();
    if(GLEW_OK != err) {
        fprintf(stderr,"glew error:%s\n",glewGetErrorString(err));
        return 1;
    }
    
    //调用SetupRC
    SetupRC();
    
    glutMainLoop();
    
    return 0;
}
